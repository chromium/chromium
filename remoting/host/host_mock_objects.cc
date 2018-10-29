// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/file_proxy_wrapper.h"
#include "remoting/host/input_injector.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/transport.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

MockDesktopEnvironment::MockDesktopEnvironment() = default;

MockDesktopEnvironment::~MockDesktopEnvironment() = default;

std::unique_ptr<ActionExecutor> MockDesktopEnvironment::CreateActionExecutor() {
  return base::WrapUnique(CreateActionExecutorPtr());
}

std::unique_ptr<AudioCapturer> MockDesktopEnvironment::CreateAudioCapturer() {
  return base::WrapUnique(CreateAudioCapturerPtr());
}

std::unique_ptr<InputInjector> MockDesktopEnvironment::CreateInputInjector() {
  return base::WrapUnique(CreateInputInjectorPtr());
}

std::unique_ptr<ScreenControls> MockDesktopEnvironment::CreateScreenControls() {
  return base::WrapUnique(CreateScreenControlsPtr());
}

std::unique_ptr<webrtc::DesktopCapturer>
MockDesktopEnvironment::CreateVideoCapturer() {
  return base::WrapUnique(CreateVideoCapturerPtr());
}

std::unique_ptr<webrtc::MouseCursorMonitor>
MockDesktopEnvironment::CreateMouseCursorMonitor() {
  return base::WrapUnique(CreateMouseCursorMonitorPtr());
}

std::unique_ptr<FileProxyWrapper>
MockDesktopEnvironment::CreateFileProxyWrapper() {
  return base::WrapUnique(CreateFileProxyWrapperPtr());
}

MockDesktopEnvironmentFactory::MockDesktopEnvironmentFactory() = default;

MockDesktopEnvironmentFactory::~MockDesktopEnvironmentFactory() = default;

std::unique_ptr<DesktopEnvironment> MockDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    const DesktopEnvironmentOptions& options) {
  return base::WrapUnique(CreatePtr());
}

MockInputInjector::MockInputInjector() = default;

MockInputInjector::~MockInputInjector() = default;

void MockInputInjector::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  StartPtr(client_clipboard.get());
}

MockClientSessionControl::MockClientSessionControl() = default;

MockClientSessionControl::~MockClientSessionControl() = default;

MockClientSessionDetails::MockClientSessionDetails() = default;

MockClientSessionDetails::~MockClientSessionDetails() = default;

MockClientSessionEventHandler::MockClientSessionEventHandler() = default;

MockClientSessionEventHandler::~MockClientSessionEventHandler() = default;

MockHostStatusObserver::MockHostStatusObserver() = default;

MockHostStatusObserver::~MockHostStatusObserver() = default;

MockSecurityKeyAuthHandler::MockSecurityKeyAuthHandler() = default;

MockSecurityKeyAuthHandler::~MockSecurityKeyAuthHandler() = default;

void MockSecurityKeyAuthHandler::SetSendMessageCallback(
    const SecurityKeyAuthHandler::SendMessageCallback& callback) {
  callback_ = callback;
}

const SecurityKeyAuthHandler::SendMessageCallback&
MockSecurityKeyAuthHandler::GetSendMessageCallback() {
  return callback_;
}

MockMouseCursorMonitor::MockMouseCursorMonitor() = default;

MockMouseCursorMonitor::~MockMouseCursorMonitor() = default;

}  // namespace remoting
