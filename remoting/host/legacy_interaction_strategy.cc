// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/legacy_interaction_strategy.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/buildflag.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/delegating_desktop_display_info_monitor.h"
#include "remoting/host/desktop_and_cursor_conditional_composer.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_capturer_wrapper.h"
#include "remoting/host/desktop_display_info_loader.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_interaction_strategy.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/host/webrtc_mouse_cursor_monitor_adaptor.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "remoting/host/chromeos/frame_sink_desktop_capturer.h"
#include "remoting/host/chromeos/mouse_cursor_monitor_aura.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "remoting/host/win/mouse_cursor_monitor_win.h"
#endif

namespace remoting {

LegacyInteractionStrategy::~LegacyInteractionStrategy() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

std::unique_ptr<ActionExecutor>
LegacyInteractionStrategy::CreateActionExecutor() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return ActionExecutor::Create();
}

std::unique_ptr<AudioCapturer>
LegacyInteractionStrategy::CreateAudioCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::Create();
}

std::unique_ptr<InputInjector>
LegacyInteractionStrategy::CreateInputInjector() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return InputInjector::Create(input_task_runner_, ui_task_runner_);
}

std::unique_ptr<DesktopResizer>
LegacyInteractionStrategy::CreateDesktopResizer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return DesktopResizer::Create();
}

std::unique_ptr<DesktopCapturer> LegacyInteractionStrategy::CreateVideoCapturer(
    webrtc::ScreenId id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner;
#if BUILDFLAG(IS_CHROMEOS)
  capture_task_runner = ui_task_runner_;
#else   // !BUILDFLAG(IS_CHROMEOS)
  // The mouse cursor monitor runs on the |video_capture_task_runner_| so the
  // desktop capturer also needs to run on that task_runner for certain
  // platforms.
  // TODO: yuweih - The comment above is not valid for Windows. Validate it for
  // other platforms.
  capture_task_runner = video_capture_task_runner_;
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if defined(REMOTING_USE_X11)
  // Workaround for http://crbug.com/1361502: Run each capturer (and
  // mouse-cursor-monitor) on a separate X11 Display.
  auto new_options = webrtc::DesktopCaptureOptions::CreateDefault();
  options_.desktop_capture_options()->set_x_display(
      std::move(new_options.x_display()));
  options_.desktop_capture_options()->x_display()->IgnoreXServerGrabs();
#endif  // REMOTING_USE_X11

  auto creator = base::BindOnce(
      [](webrtc::DesktopCaptureOptions options, webrtc::ScreenId id) {
        std::unique_ptr<webrtc::DesktopCapturer> capturer;
#if BUILDFLAG(IS_CHROMEOS)
        capturer = std::make_unique<FrameSinkDesktopCapturer>();
#else   // !BUILDFLAG(IS_CHROMEOS)
        capturer = webrtc::DesktopCapturer::CreateScreenCapturer(options);
#endif  // !BUILDFLAG(IS_CHROMEOS)
        if (capturer) {
          capturer->SelectSource(id);
        }
        return capturer;
      },
      *options_.desktop_capture_options(), id);

  std::unique_ptr<DesktopCapturer> desktop_capturer;
  if (options_.capture_video_on_dedicated_thread()) {
    auto desktop_capturer_wrapper = std::make_unique<DesktopCapturerWrapper>();
    desktop_capturer_wrapper->CreateCapturer(std::move(creator));
    desktop_capturer = std::move(desktop_capturer_wrapper);
  } else {
    auto desktop_capturer_proxy =
        std::make_unique<DesktopCapturerProxy>(std::move(capture_task_runner));
    desktop_capturer_proxy->CreateCapturer(std::move(creator));
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

std::unique_ptr<DesktopDisplayInfoMonitor>
LegacyInteractionStrategy::CreateDisplayInfoMonitor() {
  return std::make_unique<DelegatingDesktopDisplayInfoMonitor>(
      display_info_monitor_.GetWeakPtr());
}

std::unique_ptr<protocol::MouseCursorMonitor>
LegacyInteractionStrategy::CreateMouseCursorMonitor() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  auto display_monitor = std::make_unique<DelegatingDesktopDisplayInfoMonitor>(
      display_info_monitor_.GetWeakPtr());
#if BUILDFLAG(IS_WIN)
  return std::make_unique<MouseCursorMonitorWin>(std::move(display_monitor));
#else  // !BUILDFLAG(IS_WIN)
  auto creator = base::BindOnce(
      [](webrtc::DesktopCaptureOptions options)
          -> std::unique_ptr<webrtc::MouseCursorMonitor> {
#if BUILDFLAG(IS_CHROMEOS)
        return std::make_unique<MouseCursorMonitorAura>();
#else   // BUILDFLAG(IS_CHROMEOS)
        return webrtc::MouseCursorMonitor::Create(options);
#endif  // BUILDFLAG(IS_CHROMEOS)
      },
      *options_.desktop_capture_options());
  return std::make_unique<WebrtcMouseCursorMonitorAdaptor>(
      std::make_unique<MouseCursorMonitorProxy>(video_capture_task_runner_,
                                                std::move(creator)),
      std::move(display_monitor));
#endif  // !BUILDFLAG(IS_WIN)
}

std::unique_ptr<KeyboardLayoutMonitor>
LegacyInteractionStrategy::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return KeyboardLayoutMonitor::Create(std::move(callback), input_task_runner_);
}

std::unique_ptr<ActiveDisplayMonitor>
LegacyInteractionStrategy::CreateActiveDisplayMonitor(
    base::RepeatingCallback<void(webrtc::ScreenId)> callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return ActiveDisplayMonitor::Create(ui_task_runner_, std::move(callback));
}

std::unique_ptr<LocalInputMonitor>
LegacyInteractionStrategy::CreateLocalInputMonitor() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return LocalInputMonitor::Create(caller_task_runner_, input_task_runner_,
                                   ui_task_runner_);
}

std::unique_ptr<CurtainMode> LegacyInteractionStrategy::CreateCurtainMode(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return CurtainMode::Create(caller_task_runner_, ui_task_runner_,
                             client_session_control);
}

LegacyInteractionStrategy::LegacyInteractionStrategy(
    const DesktopEnvironmentOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner)
    : options_(options),
      caller_task_runner_(std::move(caller_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)),
      video_capture_task_runner_(std::move(video_capture_task_runner)),
      input_task_runner_(std::move(input_task_runner)),
      display_info_monitor_(ui_task_runner_,
                            DesktopDisplayInfoLoader::Create()) {}

LegacyInteractionStrategyFactory::LegacyInteractionStrategyFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner)
    : caller_task_runner_(std::move(caller_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)),
      video_capture_task_runner_(std::move(video_capture_task_runner)),
      input_task_runner_(std::move(input_task_runner)) {}

LegacyInteractionStrategyFactory::~LegacyInteractionStrategyFactory() = default;

void LegacyInteractionStrategyFactory::Create(
    const DesktopEnvironmentOptions& options,
    CreateCallback callback) {
  auto session = base::WrapUnique(new LegacyInteractionStrategy(
      options, caller_task_runner_, ui_task_runner_, video_capture_task_runner_,
      input_task_runner_));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(session)));
}

}  // namespace remoting
