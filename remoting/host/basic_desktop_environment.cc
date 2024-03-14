// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_and_cursor_conditional_composer.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_capturer_wrapper.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/file_transfer/local_file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/host/webauthn/remote_webauthn_extension_notifier.h"
#include "remoting/protocol/capability_names.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#include "base/logging.h"

#if BUILDFLAG(IS_WIN)
#include "remoting/host/win/evaluate_d3d.h"
#endif

#if defined(REMOTING_USE_X11)
#include "base/threading/watchdog.h"
#include "remoting/host/linux/wayland_utils.h"
#include "remoting/host/linux/x11_util.h"
#endif

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

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
class IgnoreXServerGrabsWatchdog : public base::Watchdog::Delegate {
 public:
  IgnoreXServerGrabsWatchdog()
      : watchdog_(kWaitForIgnoreXServerGrabsTimeout,
                  "IgnoreXServerGrabs Watchdog",
                  /*enabled=*/true,
                  this) {}
  ~IgnoreXServerGrabsWatchdog() override = default;

  void Arm() { watchdog_.Arm(); }

  void Alarm() override {
    // Crash the host if IgnoreXServerGrabs() takes too long.
    CHECK(false) << "IgnoreXServerGrabs() timed out.";
  }

 private:
  base::Watchdog watchdog_;
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

DesktopDisplayInfoMonitor* BasicDesktopEnvironment::GetDisplayInfoMonitor() {
  if (!display_info_monitor_) {
    using VideoLayoutCallback =
        base::RepeatingCallback<void(std::unique_ptr<protocol::VideoLayout>)>;

    VideoLayoutCallback video_layout_callback =
        base::BindRepeating(&ClientSessionControl::OnDesktopDisplayChanged,
                            client_session_control_);

    // |video_layout_callback| is bound to |client_session_control_| which is a
    // WeakPtr, but it accepts a VideoLayout proto as the parameter. DDIM needs
    // a callback that accepts a DesktopDisplayInfo& instead.
    auto converting_callback =
        base::BindRepeating([](const DesktopDisplayInfo& info) {
          return info.GetVideoLayoutProto();
        });
    DesktopDisplayInfoMonitor::Callback callback =
        std::move(converting_callback).Then(std::move(video_layout_callback));

    display_info_monitor_ =
        std::make_unique<DesktopDisplayInfoMonitor>(ui_task_runner_);
    display_info_monitor_->AddCallback(std::move(callback));
  }
  return display_info_monitor_.get();
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

std::unique_ptr<ActiveDisplayMonitor>
BasicDesktopEnvironment::CreateActiveDisplayMonitor(
    ActiveDisplayMonitor::Callback callback) {
  return ActiveDisplayMonitor::Create(ui_task_runner_, std::move(callback));
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
  return std::string();
}

void BasicDesktopEnvironment::SetCapabilities(const std::string& capabilities) {
}

uint32_t BasicDesktopEnvironment::GetDesktopSessionId() const {
  return UINT32_MAX;
}

std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
BasicDesktopEnvironment::CreateRemoteWebAuthnStateChangeNotifier() {
  return std::make_unique<RemoteWebAuthnExtensionNotifier>();
}

std::unique_ptr<DesktopCapturer> BasicDesktopEnvironment::CreateVideoCapturer(
    webrtc::ScreenId id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  capture_task_runner = ui_task_runner_;
#elif BUILDFLAG(IS_LINUX) && defined(REMOTING_USE_WAYLAND)
  // Each capturer instance should get its own thread so the capturers don't
  // compete with each other in multistream mode.
  capture_task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::HIGHEST},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
#else
  // The mouse cursor monitor runs on the |video_capture_task_runner_| so the
  // desktop capturer also needs to run on that task_runner for certain
  // platforms. For example, if we run the desktop capturer on a different
  // thread on Windows, the cursor shape won't be captured when in GDI mode.
  capture_task_runner = video_capture_task_runner_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_LINUX)

#if defined(REMOTING_USE_X11)
  if (!IsRunningWayland()) {
    // Workaround for http://crbug.com/1361502: Run each capturer (and
    // mouse-cursor-monitor) on a separate X11 Display.
    auto new_options = webrtc::DesktopCaptureOptions::CreateDefault();
    mutable_desktop_capture_options()->set_x_display(
        std::move(new_options.x_display()));
    desktop_capture_options().x_display()->IgnoreXServerGrabs();
  }
#endif  // REMOTING_USE_X11

  std::unique_ptr<DesktopCapturer> desktop_capturer;
  if (options_.capture_video_on_dedicated_thread()) {
    auto desktop_capturer_wrapper = std::make_unique<DesktopCapturerWrapper>();
    desktop_capturer_wrapper->CreateCapturer(desktop_capture_options(), id);
    desktop_capturer = std::move(desktop_capturer_wrapper);
  } else {
    auto desktop_capturer_proxy =
        std::make_unique<DesktopCapturerProxy>(std::move(capture_task_runner));
    desktop_capturer_proxy->CreateCapturer(desktop_capture_options(), id);
    desktop_capturer = std::move(desktop_capturer_proxy);
  }

#if BUILDFLAG(IS_APPLE)
  // Mac includes the mouse cursor in the captured image in curtain mode.
  if (options_.enable_curtaining()) {
    return desktop_capturer;
  }
#endif
  return std::make_unique<DesktopAndCursorConditionalComposer>(
      std::move(desktop_capturer));
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
  if (!IsRunningWayland()) {
    // TODO(yuweih): The watchdog is just to test the hypothesis.
    // The IgnoreXServerGrabs() call should probably be moved to whichever
    // thread that created desktop_capture_options().x_display().
    IgnoreXServerGrabsWatchdog watchdog;
    watchdog.Arm();
    desktop_capture_options().x_display()->IgnoreXServerGrabs();
  }
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
