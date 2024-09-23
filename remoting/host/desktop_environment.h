// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace webrtc {
class DesktopCapturer;
class MouseCursorMonitor;
}  // namespace webrtc

namespace remoting {

class ActionExecutor;
class ActiveDisplayMonitor;
class AudioCapturer;
class ClientSessionControl;
class ClientSessionEvents;
class DesktopDisplayInfoMonitor;
class FileOperations;
class InputInjector;
class KeyboardLayoutMonitor;
class RemoteWebAuthnStateChangeNotifier;
class ScreenControls;
class UrlForwarderConfigurator;

namespace protocol {
class KeyboardLayout;
}  // namespace protocol

// Provides factory methods for creation of audio/video capturers and event
// executor for a given desktop environment.
class DesktopEnvironment {
 public:
  virtual ~DesktopEnvironment() = default;

  // Factory methods used to create audio/video capturers, event executor, and
  // screen controls object for a particular desktop environment.
  virtual std::unique_ptr<ActionExecutor> CreateActionExecutor() = 0;
  virtual std::unique_ptr<AudioCapturer> CreateAudioCapturer() = 0;
  virtual std::unique_ptr<InputInjector> CreateInputInjector() = 0;
  virtual std::unique_ptr<ScreenControls> CreateScreenControls() = 0;
  virtual std::unique_ptr<DesktopCapturer> CreateVideoCapturer(
      webrtc::ScreenId id) = 0;

  // Returns a DisplayInfoMonitor that is owned by this object. Returns
  // nullptr if the implementation does not support monitoring of displays
  // (for example, in the Network process if multi-process is enabled).
  virtual DesktopDisplayInfoMonitor* GetDisplayInfoMonitor() = 0;

  virtual std::unique_ptr<webrtc::MouseCursorMonitor>
  CreateMouseCursorMonitor() = 0;
  virtual std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)>
          callback) = 0;
  virtual std::unique_ptr<ActiveDisplayMonitor> CreateActiveDisplayMonitor(
      base::RepeatingCallback<void(webrtc::ScreenId)> callback) = 0;
  virtual std::unique_ptr<FileOperations> CreateFileOperations() = 0;
  virtual std::unique_ptr<UrlForwarderConfigurator>
  CreateUrlForwarderConfigurator() = 0;
  virtual std::unique_ptr<RemoteWebAuthnStateChangeNotifier>
  CreateRemoteWebAuthnStateChangeNotifier() = 0;

  // Returns the set of all capabilities supported by |this|.
  virtual std::string GetCapabilities() const = 0;

  // Passes the final set of capabilities negotiated between the client and host
  // to |this|.
  virtual void SetCapabilities(const std::string& capabilities) = 0;

  // Returns an id which identifies the current desktop session on Windows.
  // Other platforms will always return the default value (UINT32_MAX).
  virtual uint32_t GetDesktopSessionId() const = 0;
};

// Used to create |DesktopEnvironment| instances.
class DesktopEnvironmentFactory {
 public:
  virtual ~DesktopEnvironmentFactory() = default;

  // Creates an instance of |DesktopEnvironment|. Returns a nullptr pointer if
  // the desktop environment could not be created for any reason (if the curtain
  // failed to active for instance). |client_session_control| must outlive
  // the created desktop environment.
  virtual std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<ClientSessionEvents> client_session_events,
      const DesktopEnvironmentOptions& options) = 0;

  // Returns |true| if created |DesktopEnvironment| instances support audio
  // capture.
  virtual bool SupportsAudioCapture() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
