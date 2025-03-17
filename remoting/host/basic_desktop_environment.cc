// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_capturer_wrapper.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_interaction_strategy.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/file_transfer/local_file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"
#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/webauthn/remote_webauthn_extension_notifier.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#if BUILDFLAG(IS_WIN)
#include "remoting/host/win/evaluate_d3d.h"
#endif

#if defined(REMOTING_USE_X11)
#include "base/threading/watchdog.h"
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
    NOTREACHED() << "IgnoreXServerGrabs() timed out.";
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

  return interaction_strategy_->CreateAudioCapturer();
}

std::unique_ptr<InputInjector> BasicDesktopEnvironment::CreateInputInjector() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return interaction_strategy_->CreateInputInjector();
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

    display_info_monitor_ = interaction_strategy_->CreateDisplayInfoMonitor();
    display_info_monitor_->AddCallback(std::move(callback));
  }
  return display_info_monitor_.get();
}

std::unique_ptr<webrtc::MouseCursorMonitor>
BasicDesktopEnvironment::CreateMouseCursorMonitor() {
  return interaction_strategy_->CreateMouseCursorMonitor();
}

std::unique_ptr<KeyboardLayoutMonitor>
BasicDesktopEnvironment::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return interaction_strategy_->CreateKeyboardLayoutMonitor(
      std::move(callback));
}

std::unique_ptr<ActiveDisplayMonitor>
BasicDesktopEnvironment::CreateActiveDisplayMonitor(
    ActiveDisplayMonitor::Callback callback) {
  return interaction_strategy_->CreateActiveDisplayMonitor(std::move(callback));
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

std::uint32_t BasicDesktopEnvironment::GetDesktopSessionId() const {
  return UINT32_MAX;
}

std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
BasicDesktopEnvironment::CreateRemoteWebAuthnStateChangeNotifier() {
  return std::make_unique<RemoteWebAuthnExtensionNotifier>();
}

std::unique_ptr<DesktopCapturer> BasicDesktopEnvironment::CreateVideoCapturer(
    webrtc::ScreenId id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return interaction_strategy_->CreateVideoCapturer(id);
}

BasicDesktopEnvironment::BasicDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<DesktopInteractionStrategy> interaction_strategy,
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      interaction_strategy_(std::move(interaction_strategy)),
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
#elif BUILDFLAG(IS_WIN)
  // Check whether D3D is available as long as the DirectX capturer wasn't
  // explicitly disabled. This check is necessary because the network process
  // runs in Session 0 and cannot check whether D3D is available or not so the
  // default value is set to true but can be overridden by the client.
  if (options_.desktop_capture_options()->allow_directx_capturer()) {
    options_.desktop_capture_options()->set_allow_directx_capturer(
        IsD3DAvailable());
  }
#endif
}

BasicDesktopEnvironmentFactory::BasicDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    std::unique_ptr<DesktopInteractionStrategyFactory>
        interaction_strategy_factory)
    : caller_task_runner_(std::move(caller_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)),
      interaction_strategy_factory_(std::move(interaction_strategy_factory)) {}

BasicDesktopEnvironmentFactory::~BasicDesktopEnvironmentFactory() = default;

bool BasicDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

void BasicDesktopEnvironmentFactory::CreateInteractionStrategy(
    const DesktopEnvironmentOptions& options,
    DesktopInteractionStrategyFactory::CreateCallback callback) {
  interaction_strategy_factory_->Create(options, std::move(callback));
}

}  // namespace remoting
