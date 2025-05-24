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
#include "base/notimplemented.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_loader.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_RemoteDesktop.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_ScreenCast.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_ScreenSaver.h"
#include "remoting/host/linux/ei_sender_session.h"
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
  class GnomeActionExecutor : public ActionExecutor {
   public:
    explicit GnomeActionExecutor(GDBusConnectionRef connection)
        : connection_(std::move(connection)) {}
    ~GnomeActionExecutor() override = default;
    void ExecuteAction(const protocol::ActionRequest& request) override {
      switch (request.action()) {
        case protocol::ActionRequest::LOCK_WORKSTATION:
          connection_.Call<org_gnome_ScreenSaver::Lock>(
              "org.gnome.ScreenSaver", "/org/gnome/ScreenSaver", std::tuple(),
              base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
                if (!result.has_value()) {
                  LOG(WARNING) << "Failed to lock screen: " << result.error();
                }
              }));
          break;
        default:
          break;
      }
    }

   private:
    GDBusConnectionRef connection_;
  };

  return std::make_unique<GnomeActionExecutor>(connection_);
}

std::unique_ptr<AudioCapturer> GnomeInteractionStrategy::CreateAudioCapturer() {
  // TODO(jamiewalch): Support both pipe and session capture.
  return AudioCapturer::Create();
}

std::unique_ptr<InputInjector> GnomeInteractionStrategy::CreateInputInjector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  class GnomeInputInjector : public InputInjector {
   public:
    explicit GnomeInputInjector(base::WeakPtr<GnomeInteractionStrategy> session)
        : session_(std::move(session)) {}
    ~GnomeInputInjector() override = default;

    // InputInjector implementation
    void Start(
        std::unique_ptr<protocol::ClipboardStub> client_clipboard) override {}

    // InputStub implementation
    void InjectKeyEvent(const protocol::KeyEvent& event) override {
      if (!session_) {
        return;
      }
      session_->InjectKeyEvent(event);
    }
    void InjectTextEvent(const protocol::TextEvent& event) override {
      NOTIMPLEMENTED();
    }
    void InjectMouseEvent(const protocol::MouseEvent& event) override {
      if (!session_) {
        return;
      }
      session_->InjectMouseEvent(event);
    }
    void InjectTouchEvent(const protocol::TouchEvent& event) override {
      NOTIMPLEMENTED();
    }

    // ClipboardStub implementation
    void InjectClipboardEvent(const protocol::ClipboardEvent& event) override {
      NOTIMPLEMENTED();
    }

   private:
    base::WeakPtr<GnomeInteractionStrategy> session_;
  };
  return std::make_unique<GnomeInputInjector>(weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<DesktopResizer>
GnomeInteractionStrategy::CreateDesktopResizer() {
  // TODO(jamiewalch): Actually implement.
  class GnomeDesktopResizer : public DesktopResizer {
   public:
    explicit GnomeDesktopResizer(
        base::WeakPtr<GnomeInteractionStrategy> session)
        : session_(std::move(session)) {}
    ~GnomeDesktopResizer() override = default;
    ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override {
      // TODO(jamiewalch): Expose resolution from SharedScreencastStream
      return {};
    }
    std::list<ScreenResolution> GetSupportedResolutions(
        const ScreenResolution& preferred,
        webrtc::ScreenId screen_id) override {
      return {preferred};
    }

    void SetResolution(const ScreenResolution& resolution,
                       webrtc::ScreenId screen_id) override {
      if (!session_) {
        return;
      }
      DCHECK_CALLED_ON_VALID_SEQUENCE(session_->sequence_checker_);
      session_->capture_stream_.SetResolution(resolution);
    }

    void RestoreResolution(const ScreenResolution& original,
                           webrtc::ScreenId screen_id) override {}
    void SetVideoLayout(const protocol::VideoLayout& layout) override {}

   private:
    base::WeakPtr<GnomeInteractionStrategy> session_;
  };
  return std::make_unique<GnomeDesktopResizer>(weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<DesktopCapturer> GnomeInteractionStrategy::CreateVideoCapturer(
    webrtc::ScreenId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<PipewireDesktopCapturer>(
      capture_stream_.GetWeakPtr());
}
std::unique_ptr<webrtc::MouseCursorMonitor>
GnomeInteractionStrategy::CreateMouseCursorMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<PipewireMouseCursorMonitor>(
      capture_stream_.GetWeakPtr());
}

std::unique_ptr<KeyboardLayoutMonitor>
GnomeInteractionStrategy::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  // TODO(jamiewalch): Implement
  class GnomeKeyboardLayoutMonitor : public KeyboardLayoutMonitor {
   public:
    ~GnomeKeyboardLayoutMonitor() override = default;
    void Start() override {}
  };
  return std::make_unique<GnomeKeyboardLayoutMonitor>();
}

std::unique_ptr<ActiveDisplayMonitor>
GnomeInteractionStrategy::CreateActiveDisplayMonitor(
    base::RepeatingCallback<void(webrtc::ScreenId)> callback) {
  // TODO(jamiewalch): Implement
  class GnomeActiveDisplayMonitor : public ActiveDisplayMonitor {
   public:
    ~GnomeActiveDisplayMonitor() override = default;
  };
  return std::make_unique<GnomeActiveDisplayMonitor>();
}

std::unique_ptr<DesktopDisplayInfoMonitor>
GnomeInteractionStrategy::CreateDisplayInfoMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(jamiewalch): Implement
  class GnomeDisplayInfoLoader : public DesktopDisplayInfoLoader {
   public:
    DesktopDisplayInfo GetCurrentDisplayInfo() override {
      DesktopDisplayInfo info;
      // TODO(jamiewalch):
      info.AddDisplay(
          DisplayGeometry{0, 0, 0, 1280, 960, 96, 24, true, "Default display"});
      return info;
    }
  };
  return std::make_unique<DesktopDisplayInfoMonitor>(
      ui_task_runner_, std::make_unique<GnomeDisplayInfoLoader>());
}

std::unique_ptr<LocalInputMonitor>
GnomeInteractionStrategy::CreateLocalInputMonitor() {
  // TODO(jamiewalch): Implement
  class GnomeLocalInputMonitor : public LocalInputMonitor {
   public:
    void StartMonitoringForClientSession(
        base::WeakPtr<ClientSessionControl> client_session_control) override {}
    void StartMonitoring(PointerMoveCallback on_pointer_input,
                         KeyPressedCallback on_keyboard_input,
                         base::RepeatingClosure on_error) override {}
  };
  return std::make_unique<GnomeLocalInputMonitor>();
}

GnomeInteractionStrategy::GnomeInteractionStrategy(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)), weak_ptr_factory_(this) {}

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
  HOST_LOG << "Starting Mutter remote desktop session";
  DCHECK(!init_callback_);
  init_callback_ = std::move(callback);
  GDBusConnectionRef::CreateForSessionBus(
      CheckResultAndContinue(&GnomeInteractionStrategy::OnConnectionCreated,
                             "Failed to connect to D-Bus session bus"));
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

  // Include the cursor in the Pipewire stream metadata.
  constexpr std::uint32_t kCursorModeMetadata = 2;

  connection_.Call<org_gnome_Mutter_ScreenCast_Session::RecordVirtual>(
      kScreenCastBusName, screencast_session_path_,
      std::tuple{std::array{
          std::pair{"cursor-mode", GVariantFrom(Boxed{kCursorModeMetadata})},
          std::pair{"is-platform", GVariantFrom(Boxed{true})}}},
      CheckResultAndContinue(&GnomeInteractionStrategy::OnStreamCreated,
                             "Failed to record virtual monitor"));
}

void GnomeInteractionStrategy::OnStreamCreated(
    std::tuple<gvariant::ObjectPath> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Starting initial monitor stream";
  std::tie(stream_path_) = args;

  connection_.GetProperty<org_gnome_Mutter_ScreenCast_Stream::Parameters>(
      kScreenCastBusName, stream_path_,
      CheckResultAndContinue(&GnomeInteractionStrategy::OnStreamParameters,
                             "Failed to retrieve stream parameters"));
}

void GnomeInteractionStrategy::OnStreamParameters(
    GVariantRef<"a{sv}"> parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gchar* param_str = g_variant_print(parameters.raw(), true);
  HOST_LOG << "Stream parameters: " << param_str;
  g_free(param_str);

  auto maybe_boxed_mapping_id = parameters.LookUp("mapping-id");
  if (!maybe_boxed_mapping_id.has_value()) {
    std::move(init_callback_)
        .Run(base::unexpected("mapping-id stream parameter not present"));
    return;
  }
  std::string mapping_id;
  auto destructure_result = maybe_boxed_mapping_id->TryDestructure(mapping_id);
  if (!destructure_result.has_value()) {
    std::move(init_callback_)
        .Run(base::unexpected(
            base::StrCat({" Failed to retrieve mapping-id stream parameter: ",
                          destructure_result.error().ToString()})));
    return;
  }
  // Note that both OnStreamStarted and OnPipeWireStreamAdded may invoke
  // init_callback_, but the former only does so on error and the latter
  // unsubscribes from the signal, meaning that it is guaranteed only to
  // be called once.
  stream_added_signal_ = connection_.SignalSubscribe<
      org_gnome_Mutter_ScreenCast_Stream::PipeWireStreamAdded>(
      kScreenCastBusName, stream_path_,
      base::BindRepeating(&GnomeInteractionStrategy::OnPipeWireStreamAdded,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(mapping_id)));
  connection_.Call<org_gnome_Mutter_ScreenCast_Stream::Start>(
      kScreenCastBusName, stream_path_, std::tuple(),
      CheckResultAndContinue(&GnomeInteractionStrategy::OnStreamStarted,
                             "Failed to start monitor stream"));
}

void GnomeInteractionStrategy::OnStreamStarted(std::tuple<> args) {
  // Do nothing. Still need to wait for PipeWire-stream-added signal.
}

void GnomeInteractionStrategy::OnPipeWireStreamAdded(
    std::string mapping_id,
    std::tuple<std::uint32_t> args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ensure method is only run this once.
  stream_added_signal_.release();

  capture_stream_.SetPipeWireStream(get<0>(args), kInitialResolution,
                                    mapping_id, webrtc::kInvalidPipeWireFd);

  std::move(init_callback_).Run(base::ok());
}

void GnomeInteractionStrategy::InjectKeyEvent(const protocol::KeyEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event.has_usb_keycode() || !event.has_pressed()) {
    LOG(WARNING) << "Key event with no key info";
    return;
  }
  ei_session_->InjectKeyEvent(event.usb_keycode(), event.pressed());
}

void GnomeInteractionStrategy::InjectMouseEvent(
    const protocol::MouseEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool event_sent = false;
  if (event.has_fractional_coordinate() &&
      event.fractional_coordinate().has_x() &&
      event.fractional_coordinate().has_y()) {
    ei_session_->InjectAbsolutePointerMove(capture_stream_.mapping_id(),
                                           event.fractional_coordinate().x(),
                                           event.fractional_coordinate().y());
    event_sent = true;

  } else if (event.has_delta_x() || event.has_delta_y()) {
    ei_session_->InjectRelativePointerMove(
        event.has_delta_x() ? event.delta_x() : 0,
        event.has_delta_y() ? event.delta_y() : 0);
    event_sent = true;
  }

  if (event.has_button() && event.has_button_down()) {
    ei_session_->InjectButton(event.button(), event.button_down());
    event_sent = true;
  }

  if (event.has_wheel_delta_x() || event.has_wheel_delta_y()) {
    ei_session_->InjectScrollDelta(
        event.has_wheel_delta_x() ? event.wheel_delta_x() : 0,
        event.has_wheel_delta_y() ? event.wheel_delta_y() : 0);
    event_sent = true;
  } else if (event.has_wheel_ticks_x() || event.has_wheel_ticks_y()) {
    ei_session_->InjectScrollDiscrete(
        event.has_wheel_ticks_x() ? event.wheel_ticks_x() : 0,
        event.has_wheel_ticks_y() ? event.wheel_ticks_y() : 0);
    event_sent = true;
  }

  if (event_sent) {
  } else {
    LOG(WARNING) << "Mouse event with no relevant fields";
  }
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
          LOG(ERROR) << result.error();
          std::move(callback).Run(nullptr);
          return;
        }

        std::move(callback).Run(std::move(session));
      },
      std::move(session), std::move(callback)));
}

void GnomeInteractionStrategyFactory::OnSessionInit(
    std::unique_ptr<GnomeInteractionStrategy> session,
    CreateCallback callback,
    base::expected<void, std::string> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to initialize Gnome Remote Desktop session: "
               << result.error();
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(session));
}

}  // namespace remoting
