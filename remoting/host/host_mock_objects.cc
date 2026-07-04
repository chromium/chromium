// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/input_injector.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/transport.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

MockDesktopEnvironment::MockDesktopEnvironment() = default;

MockDesktopEnvironment::~MockDesktopEnvironment() = default;

MockDesktopEnvironmentFactory::MockDesktopEnvironmentFactory() = default;

MockDesktopEnvironmentFactory::~MockDesktopEnvironmentFactory() = default;

MockInputInjector::MockInputInjector() = default;

MockInputInjector::~MockInputInjector() = default;

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
    const SecurityKeyAuthHandler::SendMessageCallback& callback,
    const void* client_id) {
  send_message_callback_ = callback;
  active_client_id_ = client_id;
}

void MockSecurityKeyAuthHandler::ClearSendMessageCallback(
    const void* client_id) {
  if (active_client_id_ == client_id) {
    send_message_callback_.Reset();
    active_client_id_ = nullptr;
  }
}

const SecurityKeyAuthHandler::SendMessageCallback&
MockSecurityKeyAuthHandler::GetSendMessageCallback() {
  return send_message_callback_;
}

base::WeakPtr<SecurityKeyAuthHandler> MockSecurityKeyAuthHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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
