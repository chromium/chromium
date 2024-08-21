// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/message_pipe.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/rtc_base/ref_count.h"

namespace remoting::protocol {

// WebrtcDataStreamAdapter implements MessagePipe for WebRTC data channels.
class WebrtcDataStreamAdapter : public MessagePipe,
                                public webrtc::DataChannelObserver {
 public:
  explicit WebrtcDataStreamAdapter(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel);

  WebrtcDataStreamAdapter(const WebrtcDataStreamAdapter&) = delete;
  WebrtcDataStreamAdapter& operator=(const WebrtcDataStreamAdapter&) = delete;

  ~WebrtcDataStreamAdapter() override;

  std::string name() { return channel_->label(); }

  // MessagePipe interface.
  void Start(EventHandler* event_handler) override;
  void Send(google::protobuf::MessageLite* message,
            base::OnceClosure done) override;

 private:
  enum class State { CONNECTING, OPEN, CLOSED };

  struct PendingMessage {
    PendingMessage(webrtc::DataBuffer buffer, base::OnceClosure done_callback);
    PendingMessage(PendingMessage&&);
    ~PendingMessage();
    PendingMessage& operator=(PendingMessage&&);

    webrtc::DataBuffer buffer;
    base::OnceClosure done_callback;
  };

  void SendMessagesIfReady();

  // webrtc::DataChannelObserver interface.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;
  void OnBufferedAmountChange(uint64_t previous_amount) override;
  bool IsOkToCallOnTheNetworkThread() override;

  // Helpers for calling EventHandler methods asynchronously.
  // webrtc::DataChannelObserver can be called synchronously mid-operation
  // (e.g., in the middle of a Send operation). As such, it is important to let
  // the stack unwind before doing any real work. Since this class doesn't
  // control the EventHandler implementation, the safest approach is always to
  // call the latter's methods asynchronously.
  void InvokeOpenEvent();
  void InvokeClosedEvent();
  void InvokeMessageEvent(std::unique_ptr<CompoundBuffer> buffer);
  void HandleIncomingMessages();

  rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;

  raw_ptr<EventHandler> event_handler_ = nullptr;

  State state_ = State::CONNECTING;

  // Queue for unhandled incoming messages.
  base::queue<std::unique_ptr<CompoundBuffer>> pending_incoming_messages_;

  // The data and done callbacks for queued but not yet sent messages.
  base::queue<PendingMessage> pending_outgoing_messages_;

  base::OnceClosure pending_open_callback_;

  base::WeakPtrFactory<WebrtcDataStreamAdapter> weak_ptr_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
