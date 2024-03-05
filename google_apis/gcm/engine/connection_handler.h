// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace net{
class StreamSocket;
}  // namespace net

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace mcs_proto {
class LoginRequest;
}

namespace gcm {

// Handles performing the protocol handshake and sending/receiving protobuf
// messages. Note that no retrying or queueing is enforced at this layer.
// Once a connection error is encountered, the ConnectionHandler will disconnect
// the socket and must be reinitialized with a new StreamSocket before
// messages can be sent/received again.
class GCM_EXPORT ConnectionHandler {
 public:
  using ProtoReceivedCallback = base::RepeatingCallback<void(
      std::unique_ptr<google::protobuf::MessageLite>)>;
  using ProtoSentCallback = base::RepeatingClosure;
  using ConnectionChangedCallback = base::RepeatingCallback<void(int)>;

  ConnectionHandler();
  virtual ~ConnectionHandler();

  // Starts a new MCS connection handshake (using |login_request|) and, upon
  // success, begins listening for incoming/outgoing messages.
  //
  // Note: It is correct and expected to call Init more than once, as connection
  // issues are encountered and new connections must be made.
  virtual void Init(const mcs_proto::LoginRequest& login_request,
                    mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream) = 0;

  // Resets the handler and any internal state. Should be called any time
  // a connection reset happens externally to the handler.
  virtual void Reset() = 0;

  // Checks that a handshake has been completed and a message is not already
  // in flight.
  virtual bool CanSendMessage() const = 0;

  // Send an MCS protobuf message. CanSendMessage() must be true.
  virtual void SendMessage(const google::protobuf::MessageLite& message) = 0;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_HANDLER_H_
