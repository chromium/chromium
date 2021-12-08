// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/keyboard_layout_monitor.h"
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

std::unique_ptr<KeyboardLayoutMonitor>
MockDesktopEnvironment::CreateKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return base::WrapUnique(CreateKeyboardLayoutMonitorPtr(std::move(callback)));
}

std::unique_ptr<FileOperations> MockDesktopEnvironment::CreateFileOperations() {
  return base::WrapUnique(CreateFileOperationsPtr());
}

std::unique_ptr<DesktopAndCursorConditionalComposer>
MockDesktopEnvironment::CreateComposingVideoCapturer() {
  return base::WrapUnique(CreateComposingVideoCapturerPtr());
}

MockDesktopEnvironmentFactory::MockDesktopEnvironmentFactory() = default;

MockDesktopEnvironmentFactory::~MockDesktopEnvironmentFactory() = default;

std::unique_ptr<DesktopEnvironment> MockDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<ClientSessionEvents> client_session_events,
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

MockClientSessionEvents::MockClientSessionEvents() = default;

MockClientSessionEvents::~MockClientSessionEvents() = default;

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

MockUrlForwarderConfigurator::MockUrlForwarderConfigurator() = default;

MockUrlForwarderConfigurator::~MockUrlForwarderConfigurator() = default;

MockChromotingSessionServices::MockChromotingSessionServices() = default;

MockChromotingSessionServices::~MockChromotingSessionServices() = default;

MockChromotingHostServicesProvider::MockChromotingHostServicesProvider() =
    default;

MockChromotingHostServicesProvider::~MockChromotingHostServicesProvider() =
    default;

}  // namespace remoting
