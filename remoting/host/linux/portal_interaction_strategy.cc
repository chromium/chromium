// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/portal_interaction_strategy.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/delegating_desktop_display_info_monitor.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/linux/clipboard_portal.h"
#include "remoting/host/linux/ei_input_injector.h"
#include "remoting/host/linux/ei_keyboard_layout_monitor.h"
#include "remoting/host/linux/pipewire_desktop_capturer.h"
#include "remoting/host/linux/pipewire_local_input_monitor.h"
#include "remoting/host/linux/pipewire_mouse_cursor_monitor.h"
#include "remoting/host/linux/portal_curtain_mode.h"
#include "remoting/host/linux/portal_desktop_resizer.h"
#include "remoting/host/linux/portal_display_info_loader.h"
#include "remoting/host/linux/portal_remote_desktop_session.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

PortalInteractionStrategy::PortalInteractionStrategy() = default;
PortalInteractionStrategy::~PortalInteractionStrategy() = default;

std::unique_ptr<ActionExecutor>
PortalInteractionStrategy::CreateActionExecutor() {
  return nullptr;
}

std::unique_ptr<AudioCapturer>
PortalInteractionStrategy::CreateAudioCapturer() {
  return AudioCapturer::Create();
}

std::unique_ptr<InputInjector>
PortalInteractionStrategy::CreateInputInjector() {
  // The EI session is guaranteed to exist, because this InteractionStrategy
  // (and DesktopEnvironment) are only returned to the caller (ClientSession)
  // after the EI session is initialized.
  DCHECK(remote_desktop_->ei_session());

  auto result = std::make_unique<EiInputInjector>(
      remote_desktop_->ei_session(),
      remote_desktop_->capture_stream_manager()->GetWeakPtr(),
      std::make_unique<ClipboardPortal>());
  remote_desktop_->ei_session()->SetInputInjector(result->GetWeakPtr());
  return result;
}

std::unique_ptr<DesktopResizer>
PortalInteractionStrategy::CreateDesktopResizer() {
  return std::make_unique<PortalDesktopResizer>(
      *remote_desktop_->capture_stream_manager());
}

std::unique_ptr<DesktopCapturer> PortalInteractionStrategy::CreateVideoCapturer(
    webrtc::ScreenId id) {
  auto stream = remote_desktop_->capture_stream_manager()->GetStream(id);
  if (!stream) {
    LOG(ERROR) << "Cannot find stream for screen ID " << id;
    return nullptr;
  }
  HOST_LOG << "Creating desktop capturer for stream ID " << id;
  auto proxy = std::make_unique<DesktopCapturerProxy>(
      base::SequencedTaskRunner::GetCurrentDefault());
  proxy->set_supports_frame_callbacks(
      PipewireDesktopCapturer::kSupportsFrameCallbacks);
  proxy->set_capturer(std::make_unique<PipewireDesktopCapturer>(stream));
  return proxy;
}

std::unique_ptr<protocol::MouseCursorMonitor>
PortalInteractionStrategy::CreateMouseCursorMonitor() {
  return std::make_unique<PipewireMouseCursorMonitor>(
      remote_desktop_->mouse_cursor_capturer());
}

std::unique_ptr<KeyboardLayoutMonitor>
PortalInteractionStrategy::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return std::make_unique<EiKeyboardLayoutMonitor>(std::move(callback));
}

std::unique_ptr<ActiveDisplayMonitor>
PortalInteractionStrategy::CreateActiveDisplayMonitor(
    base::RepeatingCallback<void(webrtc::ScreenId)> callback) {
  return nullptr;
}
std::unique_ptr<DesktopDisplayInfoMonitor>
PortalInteractionStrategy::CreateDisplayInfoMonitor() {
  return std::make_unique<DelegatingDesktopDisplayInfoMonitor>(
      remote_desktop_->display_info_monitor());
}

std::unique_ptr<LocalInputMonitor>
PortalInteractionStrategy::CreateLocalInputMonitor() {
  return std::make_unique<PipewireLocalInputMonitor>(
      *remote_desktop_->mouse_cursor_capturer());
}

std::unique_ptr<CurtainMode> PortalInteractionStrategy::CreateCurtainMode(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<PortalCurtainMode>(client_session_control);
}

void PortalInteractionStrategy::Init(InitCallback callback) {
  remote_desktop_->Init(base::BindOnce(&PortalInteractionStrategy::OnInitResult,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
}

void PortalInteractionStrategy::OnInitResult(
    InitCallback callback,
    base::expected<void, std::string> result) {
  std::move(callback).Run(std::move(result));
}

PortalInteractionStrategyFactory::PortalInteractionStrategyFactory() = default;
PortalInteractionStrategyFactory::~PortalInteractionStrategyFactory() = default;

void PortalInteractionStrategyFactory::Create(
    const DesktopEnvironmentOptions& options,
    CreateCallback callback) {
  auto session = base::WrapUnique(new PortalInteractionStrategy());
  auto* raw = session.get();
  raw->Init(base::BindOnce(
      [](std::unique_ptr<PortalInteractionStrategy> session,
         CreateCallback callback, base::expected<void, std::string> result) {
        if (!result.has_value()) {
          LOG(ERROR) << "Failed to initialize Portal Remote Desktop session: "
                     << result.error();
          std::move(callback).Run(nullptr);
          return;
        }

        HOST_LOG << "Portal Remote Desktop interaction strategy initialized.";
        std::move(callback).Run(std::move(session));
      },
      std::move(session), std::move(callback)));
}

}  // namespace remoting
