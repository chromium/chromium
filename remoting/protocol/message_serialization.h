// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility methods used for encoding and decoding the protocol
// used in Chromoting.

#ifndef REMOTING_PROTOCOL_MESSAGE_SERIALIZATION_H_
#define REMOTING_PROTOCOL_MESSAGE_SERIALIZATION_H_

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting::protocol {

template <class T>
std::unique_ptr<T> ParseMessage(CompoundBuffer* buffer) {
  std::unique_ptr<T> message(new T());
  CompoundBufferInputStream stream(buffer);
  if (!message->ParseFromZeroCopyStream(&stream)) {
    LOG(WARNING) << "Received message that is not a valid protocol buffer.";
    return nullptr;
  }
  DCHECK_EQ(stream.position(), buffer->total_bytes());
  return message;
}

// Serialize the Protocol Buffer message and provide sufficient framing for
// sending it over the wire.
// This will provide sufficient prefix and suffix for the receiver side to
// decode the message.
scoped_refptr<net::IOBufferWithSize> SerializeAndFrameMessage(
    const google::protobuf::MessageLite& msg);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_MESSAGE_SERIALIZATION_H_
