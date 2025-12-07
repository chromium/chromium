// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_remote_desktop_session.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_RemoteDesktop.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_ScreenCast.h"
#include "remoting/host/linux/gnome_desktop_display_info_monitor.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

namespace {

using gvariant::Boxed;
using gvariant::BoxedRef;
using gvariant::ObjectPath;
using gvariant::ObjectPathCStr;

constexpr char kRemoteDesktopBusName[] = "org.gnome.Mutter.RemoteDesktop";
constexpr ObjectPathCStr kRemoteDesktopObjectPath =
    "/org/gnome/Mutter/RemoteDesktop";
constexpr char kScreenCastBusName[] = "org.gnome.Mutter.ScreenCast";
constexpr ObjectPathCStr kScreenCastObjectPath = "/org/gnome/Mutter/ScreenCast";

base::FilePath GetDisplayLayoutFilePath() {
  return (base::FilePath(
      GetConfigDirectoryPath().Append(GetHostHash() + ".display_layout.pb")));
}

std::unique_ptr<protocol::VideoLayout> CreateDefaultLayout() {
  auto default_layout = std::make_unique<protocol::VideoLayout>();
  default_layout->set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);
  protocol::VideoTrackLayout* track = default_layout->add_video_track();
  track->set_position_x(0);
  track->set_position_y(0);
  track->set_width(1280);
  track->set_height(960);
  track->set_x_dpi(96);
  track->set_y_dpi(96);
  return default_layout;
}

}  // namespace

GnomeRemoteDesktopSession::GnomeRemoteDesktopSession()
    : persistent_display_layout_manager_(
          GetDisplayLayoutFilePath(),
          std::make_unique<GnomeDesktopDisplayInfoMonitor>(
              display_config_monitor_.GetWeakPtr()),
          desktop_resizer_.GetWeakPtr()) {}

GnomeRemoteDesktopSession::~GnomeRemoteDesktopSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_path_ != ObjectPath()) {
    connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::Stop>(
        kRemoteDesktopBusName, session_path_, std::tuple(),
        GDBusConnectionRef::CallCallback<std::tuple<>>(base::DoNothing()));
  }
}

// static
bool GnomeRemoteDesktopSession::IsRunningUnderGnome() {
  const char* xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
  return xdg_current_desktop &&
         std::string_view{xdg_current_desktop} == "GNOME";
}

// static
GnomeRemoteDesktopSession* GnomeRemoteDesktopSession::GetInstance() {
  static base::NoDestructor<GnomeRemoteDesktopSession> instance;
  return instance.get();
}

void GnomeRemoteDesktopSession::Init(InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialization_state_ == InitializationState::kInitialized) {
    HOST_LOG << "Mutter remote desktop session is already initialized. "
             << "Postsing a task to run the callback immediately.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::ok()));
    return;
  }

  init_callbacks_.AddUnsafe(std::move(callback));
  if (initialization_state_ == InitializationState::kNotInitialized) {
    HOST_LOG << "Starting Mutter remote desktop session";
    initialization_state_ = InitializationState::kInitializing;
    GDBusConnectionRef::CreateForSessionBus(
        CheckResultAndContinue(&GnomeRemoteDesktopSession::OnConnectionCreated,
                               "Failed to connect to D-Bus session bus"));
    display_config_client_.Init();
  }
}

template <typename SuccessType, typename String>
GDBusConnectionRef::CallCallback<SuccessType>
GnomeRemoteDesktopSession::CheckResultAndContinue(
    void (GnomeRemoteDesktopSession::*success_method)(SuccessType),
    String&& error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindOnce(
      [](base::WeakPtr<GnomeRemoteDesktopSession> that,
         decltype(success_method) success_method,
         std::string_view error_context,
         base::expected<SuccessType, Loggable> result) {
        if (!that) {
          return;
        }
        if (result.has_value()) {
          (that.get()->*success_method)(std::move(result).value());
        } else {
          that->OnInitError(error_context, std::move(result).error());
        }
      },
      weak_ptr_factory_.GetWeakPtr(), success_method,
      std::forward<String>(error_context));
}

void GnomeRemoteDesktopSession::OnInitError(std::string_view error_message,
                                            Loggable error_context) {
  OnInitError(base::StrCat({error_message, ": ", error_context.ToString()}));
}

void GnomeRemoteDesktopSession::OnInitError(std::string_view what_and_why) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initialization_state_ = InitializationState::kNotInitialized;
  init_callbacks_.Notify(base::unexpected(std::string(what_and_why)));
  DCHECK(init_callbacks_.empty());
}

void GnomeRemoteDesktopSession::OnConnectionCreated(
    GDBusConnectionRef connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_ = std::move(connection);

  headless_detector_.Start(
      connection_,
      base::BindOnce(&GnomeRemoteDesktopSession::OnHeadlessDetection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GnomeRemoteDesktopSession::OnHeadlessDetection(bool is_headless) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_headless_ = is_headless;

  // One of the gLinux patches modifies the method signature of CreateSession.
  // To ease the transition, try the patched signature if the upstream signature
  // fails.
  auto call_patched_if_failed =
      [](base::WeakPtr<GnomeRemoteDesktopSession> that,
         base::expected<std::tuple<ObjectPath>, Loggable> result) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);
        if (!result.has_value()) {
          that->connection_.Call<
              org_gnome_Mutter_RemoteDesktop::CreateSession_Patched>(
              kRemoteDesktopBusName, kRemoteDesktopObjectPath, std::tuple(true),
              base::BindOnce(
                  [](Loggable previous_error,
                     base::expected<std::tuple<ObjectPath>, Loggable> result) {
                    return result.transform_error([&previous_error](
                                                      auto new_error) {
                      // If both fail, include the first as context for
                      // the second.
                      Loggable result(new_error);
                      result.AddContext(FROM_HERE, previous_error.ToString());
                      return result;
                    });
                  },
                  std::move(result).error())
                  .Then(that->CheckResultAndContinue(
                      &GnomeRemoteDesktopSession::OnSessionCreated,
                      "Failed to create remote-desktop session")));
        } else {
          that->OnSessionCreated(std::move(result).value());
        }
      };

  connection_.Call<org_gnome_Mutter_RemoteDesktop::CreateSession>(
      kRemoteDesktopBusName, kRemoteDesktopObjectPath, std::tuple(),
      base::BindOnce(call_patched_if_failed, weak_ptr_factory_.GetWeakPtr()));
}

void GnomeRemoteDesktopSession::OnSessionCreated(std::tuple<ObjectPath> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::tie(session_path_) = args;
  // TODO(jamiewalch): While we don't close the session ourselves, the Mutter
  // remote desktop has a Close signal, which might be fired when the session is
  // closed externally. See if we need to do something with it.

  connection_.GetProperty<org_gnome_Mutter_RemoteDesktop_Session::SessionId>(
      kRemoteDesktopBusName, session_path_,
      CheckResultAndContinue(&GnomeRemoteDesktopSession::OnGotSessionId,
                             "Failed to get session ID"));
}

void GnomeRemoteDesktopSession::OnGotSessionId(std::string session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_.Call<org_gnome_Mutter_ScreenCast::CreateSession>(
      kScreenCastBusName, kScreenCastObjectPath,
      std::tuple(
          std::array{std::pair{"remote-desktop-session-id",
                               gvariant::GVariantFrom(BoxedRef(session_id))},
                     std::pair{"disable-animations",
                               gvariant::GVariantFrom(Boxed{true})}}),
      CheckResultAndContinue(
          &GnomeRemoteDesktopSession::OnScreenCastSessionCreated,
          "Failed to create screen-cast session"));
}

void GnomeRemoteDesktopSession::OnScreenCastSessionCreated(
    std::tuple<ObjectPath> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::tie(screencast_session_path_) = args;

  connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::Start>(
      kRemoteDesktopBusName, session_path_, std::tuple(),
      CheckResultAndContinue(&GnomeRemoteDesktopSession::OnSessionStarted,
                             "Failed to start remote-desktop session"));
}

void GnomeRemoteDesktopSession::OnSessionStarted(std::tuple<>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::ConnectToEIS>(
      kRemoteDesktopBusName, session_path_,
      std::tuple(gvariant::EmptyArrayOf<"{sv}">()),
      CheckResultAndContinue(&GnomeRemoteDesktopSession::OnEisFd,
                             "Failed to get EIS FD"));
}

void GnomeRemoteDesktopSession::OnEisFd(
    std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto fd_list = std::move(args.second).MakeSparse();
  auto [handle] = args.first;
  auto eis_fd = fd_list.Extract(handle);
  if (!eis_fd.is_valid()) {
    OnInitError("Failed to get EIS FD",
                Loggable(FROM_HERE, "Handle not present in FD list"));
    return;
  }
  EiSenderSession::CreateWithFd(
      std::move(eis_fd),
      CheckResultAndContinue(&GnomeRemoteDesktopSession::OnEiSession,
                             "Failed to create EI session"));
}

void GnomeRemoteDesktopSession::OnEiSession(
    std::unique_ptr<EiSenderSession> ei_session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ei_session_ = std::move(ei_session);

  display_config_subscription_ = display_config_monitor_.AddCallback(
      base::BindRepeating(&GnomeRemoteDesktopSession::OnDisplayConfigReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      /*call_with_current_config=*/true);
}

void GnomeRemoteDesktopSession::OnDisplayConfigReceived(
    const GnomeDisplayConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  display_config_subscription_.reset();
  capture_stream_manager_.Init(&connection_,
                               display_config_monitor_.GetWeakPtr(),
                               screencast_session_path_);
  // This is a hack to make IT2ME work -- in IT2ME mode, the remote desktop
  // session will be started with pre-existing monitors, so we skip starting the
  // persistent display layout manager in that case, which prevents attempts to
  // restore the display layout and creations of any virtual monitors. This,
  // however, means the display layout may not be persisted when ME2ME is set up
  // on a physical machine with physical monitors.
  // TODO: yuweih - see what to do for ME2ME on a physical machine.
  if (config.monitors.empty()) {
    persistent_display_layout_manager_.Start(CreateDefaultLayout());
  }
  initialization_state_ = InitializationState::kInitialized;
  init_callbacks_.Notify(base::ok());
  DCHECK(init_callbacks_.empty());
}

}  // namespace remoting
