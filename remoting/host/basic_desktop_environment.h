// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/desktop_environment.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ui {
class SystemInputInjectorFactory;
}

namespace webrtc {

class DesktopCaptureOptions;

}  // namespace webrtc

namespace remoting {

// Used to create audio/video capturers and event executor that work with
// the local console.
class BasicDesktopEnvironment : public DesktopEnvironment {
 public:
  ~BasicDesktopEnvironment() override;

  // DesktopEnvironment implementation.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<ScreenControls> CreateScreenControls() override;
  std::unique_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() override;
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<FileProxyWrapper> CreateFileProxyWrapper() override;
  std::string GetCapabilities() const override;
  void SetCapabilities(const std::string& capabilities) override;
  uint32_t GetDesktopSessionId() const override;

 protected:
  friend class BasicDesktopEnvironmentFactory;

  BasicDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      ui::SystemInputInjectorFactory* system_input_injector_factory,
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

  ui::SystemInputInjectorFactory* system_input_injector_factory() const {
    return system_input_injector_factory_;
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

  // Passed to InputInjector.
  ui::SystemInputInjectorFactory* system_input_injector_factory_;

  DesktopEnvironmentOptions options_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironment);
};

// Used to create |BasicDesktopEnvironment| instances.
class BasicDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  BasicDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      ui::SystemInputInjectorFactory* system_input_injector_factory);
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

  ui::SystemInputInjectorFactory* system_input_injector_factory() const {
    return system_input_injector_factory_;
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

  // Passed to the environments built.
  ui::SystemInputInjectorFactory* system_input_injector_factory_;

  DISALLOW_COPY_AND_ASSIGN(BasicDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASIC_DESKTOP_ENVIRONMENT_H_
