// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LEGACY_INTERACTION_STRATEGY_H_
#define REMOTING_HOST_LEGACY_INTERACTION_STRATEGY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/desktop_interaction_strategy.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/host/polling_desktop_display_info_monitor.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

// Transitional implementation of DesktopInteractionStrategy that copies the
// previous logic from *DesktopEnvironment.
// TODO(rkjnsn): Remove this class and the various Create() methods it calls in
// favor of having per-platform implementations of DesktopInteractionStrategy.
class LegacyInteractionStrategy : public DesktopInteractionStrategy {
 public:
  ~LegacyInteractionStrategy() override;

  // DesktopInteractionStrategy implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<DesktopResizer> CreateDesktopResizer() override;
  std::unique_ptr<DesktopCapturer> CreateVideoCapturer(
      webrtc::ScreenId id) override;
  std::unique_ptr<protocol::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
      override;
  std::unique_ptr<ActiveDisplayMonitor> CreateActiveDisplayMonitor(
      base::RepeatingCallback<void(webrtc::ScreenId)> callback) override;
  std::unique_ptr<DesktopDisplayInfoMonitor> CreateDisplayInfoMonitor()
      override;
  std::unique_ptr<LocalInputMonitor> CreateLocalInputMonitor() override;
  std::unique_ptr<CurtainMode> CreateCurtainMode(
      base::WeakPtr<ClientSessionControl> client_session_control) override;

 protected:
  LegacyInteractionStrategy(
      const DesktopEnvironmentOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner);

 private:
  DesktopEnvironmentOptions options_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  PollingDesktopDisplayInfoMonitor display_info_monitor_;

  friend class LegacyInteractionStrategyFactory;
};

class LegacyInteractionStrategyFactory
    : public DesktopInteractionStrategyFactory {
 public:
  LegacyInteractionStrategyFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner);
  ~LegacyInteractionStrategyFactory() override;
  void Create(const DesktopEnvironmentOptions& options,
              CreateCallback callback) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LEGACY_INTERACTION_STRATEGY_H_
