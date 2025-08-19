// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_interaction_strategy.h"

#include <glib.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_loader.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/linux/curtain_mode_wayland.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_RemoteDesktop.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_ScreenCast.h"
#include "remoting/host/linux/ei_sender_session.h"
#include "remoting/host/linux/gnome_action_executor.h"
#include "remoting/host/linux/gnome_desktop_display_info_monitor.h"
#include "remoting/host/linux/gnome_desktop_resizer.h"
#include "remoting/host/linux/gnome_input_injector.h"
#include "remoting/host/linux/gnome_keyboard_layout_monitor.h"
#include "remoting/host/linux/gnome_local_input_monitor.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/host/linux/pipewire_desktop_capturer.h"
#include "remoting/host/linux/pipewire_mouse_cursor_monitor.h"
#include "remoting/proto/action.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/portal/pipewire_utils.h"

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

const ScreenResolution kInitialResolution{{1280, 960}, {96, 96}};

template <typename Ret, typename Success, typename Error>
base::OnceCallback<Ret(base::expected<Success, Error>)> MakeExpectedCallback(
    base::OnceCallback<Ret(Success)> success,
    base::OnceCallback<Ret(Error)> error) {
  return base::BindOnce(
      [](base::OnceCallback<Ret(Success)> success,
         base::OnceCallback<Ret(Error)> error,
         base::expected<Success, Error> result) {
        if (result.has_value()) {
          return std::move(success).Run(result.value());
        } else {
          return std::move(error).Run(result.error());
        }
      },
      std::move(success), std::move(error));
}

}  // namespace

GnomeInteractionStrategy::~GnomeInteractionStrategy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_path_ != ObjectPath()) {
    connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::Stop>(
        kRemoteDesktopBusName, session_path_, std::tuple(),
        GDBusConnectionRef::CallCallback<std::tuple<>>(base::DoNothing()));
  }
}

std::unique_ptr<ActionExecutor>
GnomeInteractionStrategy::CreateActionExecutor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<GnomeActionExecutor>(connection_);
}

std::unique_ptr<AudioCapturer> GnomeInteractionStrategy::CreateAudioCapturer() {
  // TODO(jamiewalch): Support both pipe and session capture.
  return AudioCapturer::Create();
}

std::unique_ptr<InputInjector> GnomeInteractionStrategy::CreateInputInjector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The EI session is guaranteed to exist, because this InteractionStrategy
  // (and DesktopEnvironment) are only returned to the caller (ClientSession)
  // after the EI session is initialized.
  DCHECK(ei_session_);

  return std::make_unique<GnomeInputInjector>(
      ei_session_->GetWeakPtr(), capture_stream_manager_.GetWeakPtr(),
      connection_, session_path_);
}

std::unique_ptr<DesktopResizer>
GnomeInteractionStrategy::CreateDesktopResizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<GnomeDesktopResizer>(
      capture_stream_manager_.GetWeakPtr(),
      display_config_client_.GetWeakPtr());
}

std::unique_ptr<DesktopCapturer> GnomeInteractionStrategy::CreateVideoCapturer(
    webrtc::ScreenId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto proxy = std::make_unique<DesktopCapturerProxy>(
      base::SequencedTaskRunner::GetCurrentDefault());
  proxy->set_supports_frame_callbacks(
      PipewireDesktopCapturer::kSupportsFrameCallbacks);
  base::WeakPtr<PipewireCaptureStream> stream =
      capture_stream_manager_.GetStream(id);
  if (stream) {
    proxy->set_capturer(std::make_unique<PipewireDesktopCapturer>(stream));
  } else {
    HOST_LOG << "Video capturer for screen ID " << id
             << " will be initialized after the stream is ready.";
    pending_desktop_capturer_proxies_[id] = proxy->GetWeakPtr();
  }
  return proxy;
}

std::unique_ptr<webrtc::MouseCursorMonitor>
GnomeInteractionStrategy::CreateMouseCursorMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<PipewireMouseCursorMonitor>(
      capture_stream_manager_.GetWeakPtr());
}

std::unique_ptr<KeyboardLayoutMonitor>
GnomeInteractionStrategy::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result =
      std::make_unique<GnomeKeyboardLayoutMonitor>(std::move(callback));
  ei_session_->SetKeyboardLayoutMonitor(result->GetWeakPtr());
  return result;
}

std::unique_ptr<ActiveDisplayMonitor>
GnomeInteractionStrategy::CreateActiveDisplayMonitor(
    base::RepeatingCallback<void(webrtc::ScreenId)> callback) {
  return nullptr;
}

std::unique_ptr<DesktopDisplayInfoMonitor>
GnomeInteractionStrategy::CreateDisplayInfoMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<GnomeDesktopDisplayInfoMonitor>(
      display_config_client_.GetWeakPtr());
}

std::unique_ptr<LocalInputMonitor>
GnomeInteractionStrategy::CreateLocalInputMonitor() {
  return std::make_unique<GnomeLocalInputMonitor>();
}

std::unique_ptr<CurtainMode> GnomeInteractionStrategy::CreateCurtainMode(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<CurtainModeWayland>();
}

GnomeInteractionStrategy::GnomeInteractionStrategy(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)), weak_ptr_factory_(this) {
  capture_stream_manager_subscription_ =
      capture_stream_manager_.AddObserver(this);
}

template <typename SuccessType, typename String>
GDBusConnectionRef::CallCallback<SuccessType>
GnomeInteractionStrategy::CheckResultAndContinue(
    void (GnomeInteractionStrategy::*success_method)(SuccessType),
    String&& error_context) {
  // Unretained is sound because callback owns this.
  return base::BindOnce(
      [](GnomeInteractionStrategy* that,
         decltype(success_method) success_method,
         std::string_view error_context,
         base::expected<SuccessType, Loggable> result) {
        if (result.has_value()) {
          (that->*success_method)(std::move(result).value());
        } else {
          that->OnInitError(error_context, std::move(result).error());
        }
      },
      base::Unretained(this), success_method,
      std::forward<String>(error_context));
}

void GnomeInteractionStrategy::OnInitError(std::string_view error_message,
                                           Loggable error_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(init_callback_)
      .Run(base::unexpected(
          base::StrCat({error_message, ": ", error_context.ToString()})));
}

void GnomeInteractionStrategy::Init(
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Starting Mutter remote desktop session";
  DCHECK(!init_callback_);
  init_callback_ = std::move(callback);
  GDBusConnectionRef::CreateForSessionBus(
      CheckResultAndContinue(&GnomeInteractionStrategy::OnConnectionCreated,
                             "Failed to connect to D-Bus session bus"));
  display_config_client_.Init();
}

void GnomeInteractionStrategy::OnConnectionCreated(
    GDBusConnectionRef connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_ = std::move(connection);

  // One of the gLinux patches modifies the method signature of CreateSession.
  // To ease the transition, try the patched signature if the upstream signature
  // fails.
  auto call_patched_if_failed =
      [](GnomeInteractionStrategy* that,
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
                      // If both fail, include the first as context for the
                      // second.
                      Loggable result(new_error);
                      result.AddContext(FROM_HERE, previous_error.ToString());
                      return result;
                    });
                  },
                  std::move(result).error())
                  .Then(that->CheckResultAndContinue(
                      &GnomeInteractionStrategy::OnSessionCreated,
                      "Failed to create remote-desktop session")));
        } else {
          that->OnSessionCreated(std::move(result).value());
        }
      };

  connection_.Call<org_gnome_Mutter_RemoteDesktop::CreateSession>(
      kRemoteDesktopBusName, kRemoteDesktopObjectPath, std::tuple(),
      base::BindOnce(call_patched_if_failed, base::Unretained(this)));
}

void GnomeInteractionStrategy::OnSessionCreated(
    std::tuple<gvariant::ObjectPath> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::tie(session_path_) = args;
  // TODO(jamiewalch): Listen for Closed event.

  connection_.GetProperty<org_gnome_Mutter_RemoteDesktop_Session::SessionId>(
      kRemoteDesktopBusName, session_path_,
      CheckResultAndContinue(&GnomeInteractionStrategy::OnGotSessionId,
                             "Failed to get session ID"));
}

void GnomeInteractionStrategy::OnGotSessionId(std::string session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_.Call<org_gnome_Mutter_ScreenCast::CreateSession>(
      kScreenCastBusName, kScreenCastObjectPath,
      std::tuple(std::array{
          std::pair{"remote-desktop-session-id",
                    GVariantFrom(BoxedRef(session_id))},
          std::pair{"disable-animations", GVariantFrom(Boxed{true})}}),
      CheckResultAndContinue(
          &GnomeInteractionStrategy::OnScreenCastSessionCreated,
          "Failed to create screen-cast session"));
}

void GnomeInteractionStrategy::OnScreenCastSessionCreated(
    std::tuple<gvariant::ObjectPath> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::tie(screencast_session_path_) = args;

  connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::Start>(
      kRemoteDesktopBusName, session_path_, std::tuple(),
      CheckResultAndContinue(&GnomeInteractionStrategy::OnSessionStarted,
                             "Failed to start remote-desktop session"));
}

void GnomeInteractionStrategy::OnSessionStarted(std::tuple<>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_.Call<org_gnome_Mutter_RemoteDesktop_Session::ConnectToEIS>(
      kRemoteDesktopBusName, session_path_,
      std::tuple(gvariant::EmptyArrayOf<"{sv}">()),
      CheckResultAndContinue(&GnomeInteractionStrategy::OnEisFd,
                             "Failed to get EIS FD"));
}

void GnomeInteractionStrategy::OnEisFd(
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
      CheckResultAndContinue(&GnomeInteractionStrategy::OnEiSession,
                             "Failed to create EI session"));
}

void GnomeInteractionStrategy::OnEiSession(
    std::unique_ptr<EiSenderSession> ei_session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ei_session_ = std::move(ei_session);

  capture_stream_manager_.Init(&connection_,
                               display_config_client_.GetWeakPtr(),
                               screencast_session_path_);
  capture_stream_manager_.AddStream(
      kInitialResolution,
      base::BindOnce([](base::expected<base::WeakPtr<PipewireCaptureStream>,
                                       std::string> result) {
        // Transform the value to void, while keeping the error unchanged.
        return result.transform(
            [](base::WeakPtr<PipewireCaptureStream>) { return; });
      }).Then(std::move(init_callback_)));
}

void GnomeInteractionStrategy::OnPipewireCaptureStreamAdded(
    base::WeakPtr<PipewireCaptureStream> stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream) {
    return;
  }
  auto it = pending_desktop_capturer_proxies_.find(stream->screen_id());
  if (it == pending_desktop_capturer_proxies_.end()) {
    return;
  }
  if (it->second) {
    it->second->set_capturer(std::make_unique<PipewireDesktopCapturer>(stream));
  }
  pending_desktop_capturer_proxies_.erase(it);
}

GnomeInteractionStrategyFactory::GnomeInteractionStrategyFactory(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)) {}

GnomeInteractionStrategyFactory::~GnomeInteractionStrategyFactory() = default;

void GnomeInteractionStrategyFactory::Create(
    const DesktopEnvironmentOptions& options,
    CreateCallback callback) {
  auto session =
      base::WrapUnique(new GnomeInteractionStrategy(ui_task_runner_));
  auto* raw = session.get();
  raw->Init(base::BindOnce(
      [](std::unique_ptr<GnomeInteractionStrategy> session,
         CreateCallback callback, base::expected<void, std::string> result) {
        if (!result.has_value()) {
          LOG(ERROR) << "Failed to initialize Gnome Remote Desktop session: "
                     << result.error();
          std::move(callback).Run(nullptr);
          return;
        }

        std::move(callback).Run(std::move(session));
      },
      std::move(session), std::move(callback)));
}

}  // namespace remoting
