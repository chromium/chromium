// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_interaction_strategy.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/desktop_resizer_proxy.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/linux/clipboard_gnome.h"
#include "remoting/host/linux/curtain_mode_wayland.h"
#include "remoting/host/linux/ei_input_injector.h"
#include "remoting/host/linux/ei_keyboard_layout_monitor.h"
#include "remoting/host/linux/gnome_action_executor.h"
#include "remoting/host/linux/gnome_desktop_display_info_monitor.h"
#include "remoting/host/linux/pipewire_desktop_capturer.h"
#include "remoting/host/linux/pipewire_local_input_monitor.h"
#include "remoting/host/linux/pipewire_mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

namespace {

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
}

GnomeInteractionStrategy::GnomeInteractionStrategy(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)) {}

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
  DCHECK(remote_desktop_session_->ei_session());

  auto result = std::make_unique<EiInputInjector>(
      remote_desktop_session_->ei_session(),
      remote_desktop_session_->capture_stream_manager(),
      std::make_unique<ClipboardGnome>(
          remote_desktop_session_->connection(),
          remote_desktop_session_->session_path()));
  remote_desktop_session_->ei_session()->SetInputInjector(result->GetWeakPtr());
  return result;
}

std::unique_ptr<DesktopResizer>
GnomeInteractionStrategy::CreateDesktopResizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<DesktopResizerProxy>(
      remote_desktop_session_->desktop_resizer());
}

std::unique_ptr<DesktopCapturer> GnomeInteractionStrategy::CreateVideoCapturer(
    webrtc::ScreenId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto proxy = std::make_unique<DesktopCapturerProxy>(
      base::SequencedTaskRunner::GetCurrentDefault());
  proxy->set_supports_frame_callbacks(
      PipewireDesktopCapturer::kSupportsFrameCallbacks);
  base::WeakPtr<CaptureStream> stream =
      remote_desktop_session_->capture_stream_manager()->GetStream(id);
  if (stream) {
    proxy->set_capturer(std::make_unique<PipewireDesktopCapturer>(stream));
  } else {
    HOST_LOG << "Video capturer for screen ID " << id
             << " will be initialized after the stream is ready.";
    pending_desktop_capturer_proxies_[id] = proxy->GetWeakPtr();
  }
  return proxy;
}

std::unique_ptr<protocol::MouseCursorMonitor>
GnomeInteractionStrategy::CreateMouseCursorMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<PipewireMouseCursorMonitor>(
      remote_desktop_session_->mouse_cursor_capturer());
}

std::unique_ptr<KeyboardLayoutMonitor>
GnomeInteractionStrategy::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = std::make_unique<EiKeyboardLayoutMonitor>(std::move(callback));
  remote_desktop_session_->ei_session()->SetKeyboardLayoutMonitor(
      result->GetWeakPtr());
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
      remote_desktop_session_->display_config_monitor());
}

std::unique_ptr<LocalInputMonitor>
GnomeInteractionStrategy::CreateLocalInputMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto capturer = remote_desktop_session_->mouse_cursor_capturer();
  DCHECK(capturer);
  return std::make_unique<PipewireLocalInputMonitor>(*capturer);
}

std::unique_ptr<CurtainMode> GnomeInteractionStrategy::CreateCurtainMode(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::make_unique<CurtainModeWayland>(
      remote_desktop_session_->is_headless());
}

void GnomeInteractionStrategy::Init(InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_desktop_session_->Init(
      base::BindOnce(&GnomeInteractionStrategy::OnInitResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GnomeInteractionStrategy::OnInitResult(
    InitCallback callback,
    base::expected<void, std::string> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.has_value()) {
    capture_stream_manager_subscription_ =
        remote_desktop_session_->capture_stream_manager()->AddObserver(this);
  }
  std::move(callback).Run(std::move(result));
}

void GnomeInteractionStrategy::OnPipewireCaptureStreamAdded(
    base::WeakPtr<CaptureStream> stream) {
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
