// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_INTERACTION_STRATEGY_H_
#define REMOTING_HOST_DESKTOP_INTERACTION_STRATEGY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/active_display_monitor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

class DesktopDisplayInfoMonitor;
class LocalInputMonitor;

// Interface that encapulates interacting with a desktop environment via a
// relevant set of APIs. This enables state to be shared between implementations
// of the various individual capture, injection, et cetera interfaces. E.g., if
// a video capture API returns both the video frame and cursor shape, the
// implementation of this class would facilitate a shared capturer used by both
// the DesktopCapturer and MouseCursorMonitor implementations.
class DesktopInteractionStrategy {
 public:
  virtual ~DesktopInteractionStrategy() = default;

  // The implementations returned by the various factory methods should not
  // crash if used after the DesktopInteractionStrategy implementation is
  // destroyed, but otherwise need not do anything useful in that case. E.g.,
  // it's fine if no frames are captured and all input events are discarded once
  // the DesktopInteractionStrategy is destroyed.

  // Factory methods used to create capture, injector, and monitor objects used
  // to interact with the session. Correspond to the equivalent methods on
  // DesktopEnvironment.
  virtual std::unique_ptr<ActionExecutor> CreateActionExecutor() = 0;
  virtual std::unique_ptr<AudioCapturer> CreateAudioCapturer() = 0;
  virtual std::unique_ptr<InputInjector> CreateInputInjector() = 0;
  virtual std::unique_ptr<DesktopResizer> CreateDesktopResizer() = 0;
  virtual std::unique_ptr<DesktopCapturer> CreateVideoCapturer(
      webrtc::ScreenId id) = 0;
  virtual std::unique_ptr<webrtc::MouseCursorMonitor>
  CreateMouseCursorMonitor() = 0;
  virtual std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)>
          callback) = 0;
  virtual std::unique_ptr<ActiveDisplayMonitor> CreateActiveDisplayMonitor(
      base::RepeatingCallback<void(webrtc::ScreenId)> callback) = 0;

  // Factory methods used by DesktopEnvironment that aren't exposed directly.
  virtual std::unique_ptr<DesktopDisplayInfoMonitor>
  CreateDisplayInfoMonitor() = 0;
  virtual std::unique_ptr<LocalInputMonitor> CreateLocalInputMonitor() = 0;

 protected:
  // Wraps raw capturer in a differ wrapper if appropriate, and calls
  // SelectSource.
  static std::unique_ptr<webrtc::DesktopCapturer> CreateCapturerFromRaw(
      std::unique_ptr<webrtc::DesktopCapturer> raw_capturer,
      const webrtc::DesktopCaptureOptions&,
      webrtc::ScreenId id);
};

// Factory to create DesktopInteractionStrategy instances as needed (e.g., upon
// connection).
class DesktopInteractionStrategyFactory {
 public:
  using CreateCallback =
      base::OnceCallback<void(std::unique_ptr<DesktopInteractionStrategy>)>;

  virtual ~DesktopInteractionStrategyFactory() = default;

  // Asynchronously creates a DesktopInteractionStrategy instance.
  //
  // This may involve negotiating a remote desktop session with the desktop
  // environment or otherwise setting up shared resources. The resulting session
  // object will be posted back on the same sequence, or nullptr if an error
  // occurs.
  virtual void Create(const DesktopEnvironmentOptions& options,
                      CreateCallback callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_INTERACTION_STRATEGY_H_
