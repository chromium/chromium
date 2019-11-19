// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_IMPL_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/engine/connection_handler.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace mcs_proto {
class LoginRequest;
}  // namespace mcs_proto

namespace gcm {

class SocketInputStream;
class SocketOutputStream;

class GCM_EXPORT ConnectionHandlerImpl : public ConnectionHandler {
 public:
  // Must be called on |io_task_runner|.
  // |io_task_runner|: for running IO tasks. When provided, it could be a
  //     wrapper on top of base::ThreadTaskRunnerHandle::Get() to provide power
  //     management featueres so that a delayed task posted to it can wake the
  //     system up from sleep to perform the task.
  // |read_callback| will be invoked with the contents of any received protobuf
  // message.
  // |write_callback| will be invoked anytime a message has been successfully
  // sent. Note: this just means the data was sent to the wire, not that the
  // other end received it.
  // |connection_callback| will be invoked with any fatal read/write errors
  // encountered.
  ConnectionHandlerImpl(scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                        base::TimeDelta read_timeout,
                        const ProtoReceivedCallback& read_callback,
                        const ProtoSentCallback& write_callback,
                        const ConnectionChangedCallback& connection_callback);
  ~ConnectionHandlerImpl() override;

  // ConnectionHandler implementation.
  void Init(const mcs_proto::LoginRequest& login_request,
            mojo::ScopedDataPipeConsumerHandle receive_stream,
            mojo::ScopedDataPipeProducerHandle send_stream) override;
  void Reset() override;
  bool CanSendMessage() const override;
  void SendMessage(const google::protobuf::MessageLite& message) override;

 private:
  // State machine for handling incoming data. See WaitForData(..) for usage.
  enum ProcessingState {
    // Processing the version, tag, and size packets (assuming minimum length
    // size packet). Only used during the login handshake.
    MCS_VERSION_TAG_AND_SIZE = 0,
    // Processing the tag and size packets (assuming minimum length size
    // packet). Used for normal messages.
    MCS_TAG_AND_SIZE,
    // Processing the size packet alone.
    MCS_SIZE,
    // Processing the protocol buffer bytes (for those messages with non-zero
    // sizes).
    MCS_PROTO_BYTES
  };

  // Sends the protocol version and login request. First step in the MCS
  // connection handshake.
  void Login(const google::protobuf::MessageLite& login_request);

  // SendMessage continuation. Invoked when Socket::Write completes.
  void OnMessageSent();

  // Starts the message processing process, which is comprised of the tag,
  // message size, and bytes packet types.
  void GetNextMessage();

  // Performs any necessary SocketInputStream refreshing until the data
  // associated with |packet_type| is fully ready, then calls the appropriate
  // OnGot* message to process the packet data. If the read times out,
  // will close the stream and invoke the connection callback.
  void WaitForData(ProcessingState state);

  // Incoming data helper methods.
  void OnGotVersion();
  void OnGotMessageTag();
  void OnGotMessageSize();
  void OnGotMessageBytes();

  // Timeout handler.
  void OnTimeout();

  // Closes the current connection.
  void CloseConnection();

  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Timeout policy: the timeout is only enforced while waiting on the
  // handshake (version and/or LoginResponse) or once at least a tag packet has
  // been received. It is reset every time new data is received, and is
  // only stopped when a full message is processed.
  // TODO(zea): consider enforcing a separate timeout when waiting for
  // a message to send.
  const base::TimeDelta read_timeout_;
  base::OneShotTimer read_timeout_timer_;

  // This connection's input/output streams.
  std::unique_ptr<SocketInputStream> input_stream_;
  std::unique_ptr<SocketOutputStream> output_stream_;

  // Whether the MCS login handshake has successfully completed. See Init(..)
  // description for more info on what the handshake involves.
  bool handshake_complete_;

  // State for the message currently being processed, if there is one.
  uint8_t message_tag_;
  uint32_t message_size_;

  ProtoReceivedCallback read_callback_;
  ProtoSentCallback write_callback_;
  ConnectionChangedCallback connection_callback_;

  // The number of bytes of the size packet read so far without finishing the
  // read. This can be up to but no larger than 5 (the max number of bytes for
  // a varint32).
  uint8_t size_packet_so_far_;
  // A temporary input buffer for holding large messages. Will hold
  // message_size_ bytes for messages larger than the normal size limit (and
  // will be empty otherwise).
  std::vector<uint8_t> payload_input_buffer_;

  base::WeakPtrFactory<ConnectionHandlerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConnectionHandlerImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_IMPL_H_
