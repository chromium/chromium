// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_webrtc_connection.h"

#include "components/webrtc/thread_wrapper.h"
#include "remoting/base/errors.h"
#include "remoting/base/logging.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {
namespace test {

FakeWebrtcConnection::FakeWebrtcConnection(
    scoped_refptr<protocol::TransportContext> transport_context,
    base::OnceClosure on_closed) {
  // TODO(lambroslambrou): Passing nullptr for the VideoEncoderFactory may
  // break the ftl_signaling_playground executable. If needed, this should be
  // replaced with a factory that supports at least one video codec.
  transport_ = std::make_unique<protocol::WebrtcTransport>(
      webrtc::ThreadWrapper::current(), transport_context, nullptr, this);
  on_closed_ = std::move(on_closed);
}

FakeWebrtcConnection::~FakeWebrtcConnection() = default;

void FakeWebrtcConnection::OnWebrtcTransportConnecting() {
  HOST_LOG << "Webrtc transport is connecting...";
}

void FakeWebrtcConnection::OnWebrtcTransportConnected() {
  HOST_LOG << "Webrtc transport is connected!!!";
  std::move(on_closed_).Run();
}

void FakeWebrtcConnection::OnWebrtcTransportError(protocol::ErrorCode error) {
  LOG(ERROR) << "Webrtc transport error: " << ErrorCodeToString(error);
  std::move(on_closed_).Run();
}

void FakeWebrtcConnection::OnWebrtcTransportProtocolChanged() {}

void FakeWebrtcConnection::OnWebrtcTransportIncomingDataChannel(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe) {}

void FakeWebrtcConnection::OnWebrtcTransportMediaStreamAdded(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

void FakeWebrtcConnection::OnWebrtcTransportMediaStreamRemoved(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

void FakeWebrtcConnection::OnWebrtcTransportRouteChanged(
    const protocol::TransportRoute& route) {}
}  // namespace test
}  // namespace remoting
