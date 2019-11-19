// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_webrtc_connection.h"

#include "jingle/glue/thread_wrapper.h"
#include "remoting/base/logging.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {
namespace test {

FakeWebrtcConnection::FakeWebrtcConnection(
    scoped_refptr<protocol::TransportContext> transport_context,
    base::OnceClosure on_closed) {
  transport_ = std::make_unique<protocol::WebrtcTransport>(
      jingle_glue::JingleThreadWrapper::current(), transport_context, this);
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
  LOG(ERROR) << "Webrtc transport error: " << error;
  std::move(on_closed_).Run();
}

void FakeWebrtcConnection::OnWebrtcTransportIncomingDataChannel(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe) {}

void FakeWebrtcConnection::OnWebrtcTransportMediaStreamAdded(
    scoped_refptr<webrtc::MediaStreamInterface> stream) {}

void FakeWebrtcConnection::OnWebrtcTransportMediaStreamRemoved(
    scoped_refptr<webrtc::MediaStreamInterface> stream) {}

}  // namespace test
}  // namespace remoting
