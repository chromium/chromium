// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_FAKE_CONNECTION_TO_HOST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/protocol_mock_objects.h"

namespace remoting::test {

class FakeConnectionToHost : public protocol::ConnectionToHost {
 public:
  FakeConnectionToHost();

  FakeConnectionToHost(const FakeConnectionToHost&) = delete;
  FakeConnectionToHost& operator=(const FakeConnectionToHost&) = delete;

  ~FakeConnectionToHost() override;

  // ConnectionToHost interface.
  void set_client_stub(protocol::ClientStub* client_stub) override;
  void set_clipboard_stub(protocol::ClipboardStub* clipboard_stub) override;
  void set_video_renderer(protocol::VideoRenderer* video_renderer) override;
  void InitializeAudio(
      scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
      base::WeakPtr<protocol::AudioStub> audio_stub) override;
  void Connect(std::unique_ptr<protocol::Session> session,
               scoped_refptr<protocol::TransportContext> transport_context,
               HostEventCallback* event_callback) override;
  void Disconnect(protocol::ErrorCode error) override;
  const protocol::SessionConfig& config() override;
  protocol::ClipboardStub* clipboard_forwarder() override;
  protocol::HostStub* host_stub() override;
  protocol::InputStub* input_stub() override;
  State state() const override;

  // Calls OnConnectionState on the |event_callback_| with the supplied state
  // and error.
  void SignalStateChange(protocol::Session::State state,
                         protocol::ErrorCode error);

  // Calls OnConnectionReady on the |event_callback_| with the supplied bool.
  void SignalConnectionReady(bool ready);

 private:
  void SetState(State state, protocol::ErrorCode error);

  State state_ = INITIALIZING;

  raw_ptr<HostEventCallback> event_callback_;

  testing::NiceMock<protocol::MockClipboardStub> mock_clipboard_stub_;
  testing::NiceMock<protocol::MockHostStub> mock_host_stub_;
  testing::NiceMock<protocol::MockInputStub> mock_input_stub_;
  std::unique_ptr<protocol::SessionConfig> session_config_;
};

}  // namespace remoting::test

#endif  // REMOTING_PROTOCOL_FAKE_CONNECTION_TO_HOST_H_
