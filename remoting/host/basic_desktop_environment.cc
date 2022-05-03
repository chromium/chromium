// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/file_transfer/local_file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/protocol/capability_names.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#if BUILDFLAG(IS_WIN)
#include "remoting/host/win/evaluate_d3d.h"
#endif

#if defined(REMOTING_USE_X11)
#include "base/threading/watchdog.h"
#include "remoting/host/linux/x11_util.h"
#endif

namespace remoting {

#if defined(REMOTING_USE_X11)

namespace {

// The maximum amount of time we will wait for the IgnoreXServerGrabs() to
// return before we crash the host.
constexpr base::TimeDelta kWaitForIgnoreXServerGrabsTimeout = base::Seconds(30);

// Helper class to monitor the call to
// webrtc::SharedXDisplay::IgnoreXServerGrabs() (on a temporary thread), which
// has been observed to occasionally hang forever and zombify the host.
// This class crashes the host if the IgnoreXServerGrabs() call takes too long,
// so that the ME2ME daemon process can respawn the host.
// See: crbug.com/1130090
class IgnoreXServerGrabsWatchdog : public base::Watchdog {
 public:
  IgnoreXServerGrabsWatchdog()
      : base::Watchdog(kWaitForIgnoreXServerGrabsTimeout,
                       "IgnoreXServerGrabs Watchdog",
                       /* enabled= */ true) {}
  ~IgnoreXServerGrabsWatchdog() override = default;

  void Alarm() override {
    // Crash the host if IgnoreXServerGrabs() takes too long.
    CHECK(false) << "IgnoreXServerGrabs() timed out.";
  }
};

}  // namespace

#endif  // defined(REMOTING_USE_X11)

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

std::unique_ptr<DesktopDisplayInfoMonitor>
BasicDesktopEnvironment::CreateDisplayInfoMonitor() {
  return std::make_unique<DesktopDisplayInfoMonitor>(ui_task_runner_,
                                                     client_session_control_);
}

std::unique_ptr<webrtc::MouseCursorMonitor>
BasicDesktopEnvironment::CreateMouseCursorMonitor() {
  return std::make_unique<MouseCursorMonitorProxy>(video_capture_task_runner_,
                                                   desktop_capture_options());
}

std::unique_ptr<KeyboardLayoutMonitor>
BasicDesktopEnvironment::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return KeyboardLayoutMonitor::Create(std::move(callback), input_task_runner_);
}

std::unique_ptr<FileOperations>
BasicDesktopEnvironment::CreateFileOperations() {
  return std::make_unique<LocalFileOperations>(ui_task_runner_);
}

std::unique_ptr<UrlForwarderConfigurator>
BasicDesktopEnvironment::CreateUrlForwarderConfigurator() {
  return UrlForwarderConfigurator::Create();
}

std::string BasicDesktopEnvironment::GetCapabilities() const {
  // This capability is added here because it is not supported by
  // multi-process hosts, so it should not be returned by the
  // overridden method IpcDesktopEnvironment::GetCapabilities().
  //
  // TODO(lambroslambrou): When this feature is working for
  // multi-process hosts, move this capability from here to
  // ClientSession::OnConnectionAuthenticated().
  return protocol::kMultiStreamCapability;
}

void BasicDesktopEnvironment::SetCapabilities(const std::string& capabilities) {
}

uint32_t BasicDesktopEnvironment::GetDesktopSessionId() const {
  return UINT32_MAX;
}

std::unique_ptr<DesktopAndCursorConditionalComposer>
BasicDesktopEnvironment::CreateComposingVideoCapturer(
    std::unique_ptr<DesktopDisplayInfoMonitor> monitor) {
#if BUILDFLAG(IS_APPLE)
  // Mac includes the mouse cursor in the captured image in curtain mode.
  if (options_.enable_curtaining())
    return nullptr;
#endif
  return std::make_unique<DesktopAndCursorConditionalComposer>(
      CreateVideoCapturer(std::move(monitor)));
}

std::unique_ptr<DesktopCapturer> BasicDesktopEnvironment::CreateVideoCapturer(
    std::unique_ptr<DesktopDisplayInfoMonitor> monitor) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  auto result = std::make_unique<DesktopCapturerProxy>(
      video_capture_task_runner_, ui_task_runner_);
  result->set_desktop_display_info_monitor(std::move(monitor));
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
#if defined(REMOTING_USE_X11)
  // TODO(yuweih): The watchdog is just to test the hypothesis.
  // The IgnoreXServerGrabs() call should probably be moved to whichever
  // thread that created desktop_capture_options().x_display().
  IgnoreXServerGrabsWatchdog watchdog;
  watchdog.Arm();
  desktop_capture_options().x_display()->IgnoreXServerGrabs();
  watchdog.Disarm();
#elif BUILDFLAG(IS_WIN)
  options_.desktop_capture_options()->set_allow_directx_capturer(
      IsD3DAvailable());
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
