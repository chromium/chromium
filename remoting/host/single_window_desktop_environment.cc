// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/single_window_desktop_environment.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/single_window_input_injector.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

// Enables capturing and streaming of windows.
class SingleWindowDesktopEnvironment : public BasicDesktopEnvironment {

 public:
  ~SingleWindowDesktopEnvironment() override;

  // DesktopEnvironment interface.
  std::unique_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;

 protected:
  friend class SingleWindowDesktopEnvironmentFactory;
  SingleWindowDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      webrtc::DesktopCapturer::SourceId window_id,
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options);

 private:
  webrtc::DesktopCapturer::SourceId window_id_;

  DISALLOW_COPY_AND_ASSIGN(SingleWindowDesktopEnvironment);
};

SingleWindowDesktopEnvironment::~SingleWindowDesktopEnvironment() = default;

std::unique_ptr<webrtc::DesktopCapturer>
SingleWindowDesktopEnvironment::CreateVideoCapturer() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();
  options.set_use_update_notifications(true);

  std::unique_ptr<webrtc::DesktopCapturer> window_capturer(
      webrtc::DesktopCapturer::CreateWindowCapturer(options));
  window_capturer->SelectSource(window_id_);

  return window_capturer;
}

std::unique_ptr<InputInjector>
SingleWindowDesktopEnvironment::CreateInputInjector() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  std::unique_ptr<InputInjector> input_injector(
      InputInjector::Create(input_task_runner(), ui_task_runner()));
  return SingleWindowInputInjector::CreateForWindow(
             window_id_, std::move(input_injector));
}

SingleWindowDesktopEnvironment::SingleWindowDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    webrtc::WindowId window_id,
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options)
    : BasicDesktopEnvironment(caller_task_runner,
                              video_capture_task_runner,
                              input_task_runner,
                              ui_task_runner,
                              client_session_control,
                              options),
      window_id_(window_id) {}

SingleWindowDesktopEnvironmentFactory::SingleWindowDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    webrtc::WindowId window_id)
    : BasicDesktopEnvironmentFactory(caller_task_runner,
                                     video_capture_task_runner,
                                     input_task_runner,
                                     ui_task_runner),
      window_id_(window_id) {}

SingleWindowDesktopEnvironmentFactory::
    ~SingleWindowDesktopEnvironmentFactory() = default;

std::unique_ptr<DesktopEnvironment>
SingleWindowDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return base::WrapUnique(new SingleWindowDesktopEnvironment(
      caller_task_runner(), video_capture_task_runner(), input_task_runner(),
      ui_task_runner(), window_id_, client_session_control, options));
}

}  // namespace remoting
