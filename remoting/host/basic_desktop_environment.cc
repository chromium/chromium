// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/file_transfer/local_file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/host/screen_controls.h"
#include "remoting/protocol/capability_names.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#if defined(OS_WIN)
#include "remoting/host/win/evaluate_d3d.h"
#endif

#if defined(USE_X11)
#include "remoting/host/linux/x11_util.h"
#endif

namespace remoting {

BasicDesktopEnvironment::~BasicDesktopEnvironment() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

std::unique_ptr<ActionExecutor>
BasicDesktopEnvironment::CreateActionExecutor() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Connection mode derivations (It2Me/Me2Me) should override this method and
  // return an executor instance if applicable.
  return nullptr;
}

std::unique_ptr<AudioCapturer> BasicDesktopEnvironment::CreateAudioCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::Create();
}

std::unique_ptr<InputInjector> BasicDesktopEnvironment::CreateInputInjector() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return InputInjector::Create(input_task_runner(), ui_task_runner());
}

std::unique_ptr<ScreenControls>
BasicDesktopEnvironment::CreateScreenControls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return nullptr;
}

std::unique_ptr<webrtc::MouseCursorMonitor>
BasicDesktopEnvironment::CreateMouseCursorMonitor() {
  return std::make_unique<MouseCursorMonitorProxy>(video_capture_task_runner_,
                                                   desktop_capture_options());
}

std::unique_ptr<FileOperations>
BasicDesktopEnvironment::CreateFileOperations() {
  return std::make_unique<LocalFileOperations>(ui_task_runner_);
}

std::string BasicDesktopEnvironment::GetCapabilities() const {
  return std::string();
}

void BasicDesktopEnvironment::SetCapabilities(const std::string& capabilities) {
}

uint32_t BasicDesktopEnvironment::GetDesktopSessionId() const {
  return UINT32_MAX;
}

std::unique_ptr<webrtc::DesktopCapturer>
BasicDesktopEnvironment::CreateVideoCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<DesktopCapturerProxy> result(new DesktopCapturerProxy(
      video_capture_task_runner_, client_session_control_));
  result->CreateCapturer(desktop_capture_options());
  return std::move(result);
}

BasicDesktopEnvironment::BasicDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options)
    : caller_task_runner_(caller_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner),
      client_session_control_(client_session_control),
      options_(options) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
#if defined(USE_X11)
  IgnoreXServerGrabs(desktop_capture_options().x_display()->display(), true);
#elif defined(OS_WIN)
  // The options passed to this instance are determined by a process running in
  // Session 0.  Access to DirectX functions in Session 0 is limited so the
  // results are not guaranteed to be accurate in the desktop context.  Due to
  // this problem, we need to requery the following method to make sure we are
  // still safe to use D3D APIs.  Only overwrite the value if it isn't safe to
  // use D3D APIs as we don't want to re-enable this setting if it was disabled
  // via an experiment or client flag.
  if (!IsD3DAvailable())
    options_.desktop_capture_options()->set_allow_directx_capturer(false);
#endif
}

BasicDesktopEnvironmentFactory::BasicDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : caller_task_runner_(caller_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner) {}

BasicDesktopEnvironmentFactory::~BasicDesktopEnvironmentFactory() = default;

bool BasicDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

}  // namespace remoting
