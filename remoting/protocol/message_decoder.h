// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MESSAGE_DECODER_H_
#define REMOTING_PROTOCOL_MESSAGE_DECODER_H_

#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting::protocol {

// MessageDecoder uses CompoundBuffer to split the data received from the
// network into separate messages. Each message is expected to be decoded in the
// stream as follows:
//   +--------------+--------------+
//   | message_size | message_data |
//   +--------------+--------------+
//
// Here, message_size is 4-byte integer that represents size of message_data in
// bytes. message_data - content of the message.
class MessageDecoder {
 public:
  MessageDecoder();
  virtual ~MessageDecoder();

  // Add next chunk of data. MessageDecoder retains |data| until all its bytes
  // are consumed.
  void AddData(scoped_refptr<net::IOBuffer> data, int data_size);

  // Returns next message from the stream. Ownership of the result is passed to
  // the caller. Returns nullptr if there are no complete messages yet,
  // otherwise returns a buffer that contains one message.
  CompoundBuffer* GetNextMessage();

 private:
  // Retrieves the read payload size of the current protocol buffer via |size|.
  // Returns false and leaves |size| unmodified, if we do not have enough data
  // to retrieve the current size.
  bool GetPayloadSize(int* size);

  CompoundBuffer buffer_;

  // |next_payload_| stores the size of the next payload if known.
  // |next_payload_known_| is true if the size of the next payload is known.
  // After one payload is read this is reset to false.
  int next_payload_ = 0;
  bool next_payload_known_ = false;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_MESSAGE_DECODER_H_
