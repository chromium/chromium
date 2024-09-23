// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_connection_to_host.h"

#include "base/task/single_thread_task_runner.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/transport_context.h"

namespace remoting::test {

FakeConnectionToHost::FakeConnectionToHost()
    : session_config_(protocol::SessionConfig::ForTest()) {}
FakeConnectionToHost::~FakeConnectionToHost() = default;

void FakeConnectionToHost::set_client_stub(protocol::ClientStub* client_stub) {}

void FakeConnectionToHost::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {}

void FakeConnectionToHost::set_video_renderer(
    protocol::VideoRenderer* video_renderer) {}

void FakeConnectionToHost::InitializeAudio(
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    base::WeakPtr<protocol::AudioStub> audio_stub) {}

void FakeConnectionToHost::Connect(
    std::unique_ptr<protocol::Session> session,
    scoped_refptr<protocol::TransportContext> transport_context,
    HostEventCallback* event_callback) {
  DCHECK(event_callback);

  event_callback_ = event_callback;

  SetState(CONNECTING, ErrorCode::OK);
}

void FakeConnectionToHost::Disconnect(protocol::ErrorCode error) {}

void FakeConnectionToHost::SignalStateChange(protocol::Session::State state,
                                             protocol::ErrorCode error) {
  DCHECK(event_callback_);

  switch (state) {
    case protocol::Session::INITIALIZING:
    case protocol::Session::CONNECTING:
    case protocol::Session::ACCEPTING:
    case protocol::Session::AUTHENTICATING:
      // No updates for these events.
      break;

    case protocol::Session::ACCEPTED:
      SetState(CONNECTED, error);
      break;

    case protocol::Session::AUTHENTICATED:
      SetState(AUTHENTICATED, error);
      break;

    case protocol::Session::CLOSED:
      SetState(CLOSED, error);
      break;

    case protocol::Session::FAILED:
      DCHECK(error != protocol::ErrorCode::OK);
      SetState(FAILED, error);
      break;
  }
}

void FakeConnectionToHost::SignalConnectionReady(bool ready) {
  DCHECK(event_callback_);

  event_callback_->OnConnectionReady(ready);
}

const protocol::SessionConfig& FakeConnectionToHost::config() {
  return *session_config_;
}

protocol::ClipboardStub* FakeConnectionToHost::clipboard_forwarder() {
  return &mock_clipboard_stub_;
}

protocol::HostStub* FakeConnectionToHost::host_stub() {
  return &mock_host_stub_;
}

protocol::InputStub* FakeConnectionToHost::input_stub() {
  return &mock_input_stub_;
}

protocol::ConnectionToHost::State FakeConnectionToHost::state() const {
  return state_;
}

void FakeConnectionToHost::SetState(State state, protocol::ErrorCode error) {
  // |error| should be specified only when |state| is set to FAILED.
  DCHECK(state == FAILED || error == ErrorCode::OK);

  if (state != state_) {
    state_ = state;
    event_callback_->OnConnectionState(state_, error);
  }
}

}  // namespace remoting::test
