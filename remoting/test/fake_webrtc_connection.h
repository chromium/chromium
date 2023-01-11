// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_WEBRTC_CONNECTION_H_
#define REMOTING_TEST_FAKE_WEBRTC_CONNECTION_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/protocol/webrtc_transport.h"

namespace remoting {
namespace test {

class FakeWebrtcConnection final
    : public protocol::WebrtcTransport::EventHandler {
 public:
  FakeWebrtcConnection(
      scoped_refptr<protocol::TransportContext> transport_context,
      base::OnceClosure on_closed);

  FakeWebrtcConnection(const FakeWebrtcConnection&) = delete;
  FakeWebrtcConnection& operator=(const FakeWebrtcConnection&) = delete;

  ~FakeWebrtcConnection() override;

  protocol::Transport* transport() { return transport_.get(); }

 private:
  // protocol::WebrtcTransport::EventHandler implementations.
  void OnWebrtcTransportConnecting() override;
  void OnWebrtcTransportConnected() override;
  void OnWebrtcTransportError(protocol::ErrorCode error) override;
  void OnWebrtcTransportProtocolChanged() override;
  void OnWebrtcTransportIncomingDataChannel(
      const std::string& name,
      std::unique_ptr<protocol::MessagePipe> pipe) override;
  void OnWebrtcTransportMediaStreamAdded(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnWebrtcTransportMediaStreamRemoved(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnWebrtcTransportRouteChanged(
      const protocol::TransportRoute& route) override;

  std::unique_ptr<protocol::WebrtcTransport> transport_;
  base::OnceClosure on_closed_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_WEBRTC_CONNECTION_H_
