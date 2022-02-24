// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/desktop_environment.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace webrtc {

class DesktopCaptureOptions;

}  // namespace webrtc

namespace remoting {

// Used to create audio/video capturers and event executor that work with
// the local console.
class BasicDesktopEnvironment : public DesktopEnvironment {
 public:
  BasicDesktopEnvironment(const BasicDesktopEnvironment&) = delete;
  BasicDesktopEnvironment& operator=(const BasicDesktopEnvironment&) = delete;

  ~BasicDesktopEnvironment() override;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<ScreenControls> CreateScreenControls() override;
  std::unique_ptr<webrtc::DesktopCapturer> CreateVideoCapturer(
      std::unique_ptr<DesktopDisplayInfoMonitor> monitor) override;
  std::unique_ptr<DesktopDisplayInfoMonitor> CreateDisplayInfoMonitor()
      override;
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
      override;
  std::unique_ptr<FileOperations> CreateFileOperations() override;
  std::unique_ptr<UrlForwarderConfigurator> CreateUrlForwarderConfigurator()
      override;
  std::string GetCapabilities() const override;
  void SetCapabilities(const std::string& capabilities) override;
  uint32_t GetDesktopSessionId() const override;
  std::unique_ptr<DesktopAndCursorConditionalComposer>
  CreateComposingVideoCapturer(
      std::unique_ptr<DesktopDisplayInfoMonitor> monitor) override;

 protected:
  friend class BasicDesktopEnvironmentFactory;

  BasicDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner()
      const {
    return video_capture_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

  webrtc::DesktopCaptureOptions* mutable_desktop_capture_options() {
    return options_.desktop_capture_options();
  }

  const webrtc::DesktopCaptureOptions& desktop_capture_options() const {
    return *options_.desktop_capture_options();
  }

  const DesktopEnvironmentOptions& desktop_environment_options() const {
    return options_;
  }

 private:
  // Task runner on which methods of DesktopEnvironment interface should be
  // called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to run video capturer.
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Used to send messages directly to the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  DesktopEnvironmentOptions options_;
};

// Used to create |BasicDesktopEnvironment| instances.
class BasicDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  BasicDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  BasicDesktopEnvironmentFactory(const BasicDesktopEnvironmentFactory&) =
      delete;
  BasicDesktopEnvironmentFactory& operator=(
      const BasicDesktopEnvironmentFactory&) = delete;

  ~BasicDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory implementation.
  bool SupportsAudioCapture() const override;

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner()
      const {
    return video_capture_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

 private:
  // Task runner on which methods of DesktopEnvironmentFactory interface should
  // be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to run video capture tasks.
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // Used to run input-related tasks.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Used to run UI code.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
