// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_decoder.h"

#include <stdint.h>

#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/internal.pb.h"
#include "third_party/webrtc/rtc_base/byte_order.h"

namespace remoting::protocol {

MessageDecoder::MessageDecoder() = default;

MessageDecoder::~MessageDecoder() = default;

void MessageDecoder::AddData(scoped_refptr<net::IOBuffer> data, int data_size) {
  buffer_.Append(std::move(data), data_size);
}

CompoundBuffer* MessageDecoder::GetNextMessage() {
  // Determine the payload size. If we already know it then skip this part.
  // We may not have enough data to determine the payload size so use a
  // utility function to find out.
  int next_payload = -1;
  if (!next_payload_known_ && GetPayloadSize(&next_payload)) {
    DCHECK_NE(-1, next_payload);
    next_payload_ = next_payload;
    next_payload_known_ = true;
  }

  // If the next payload size is still not known or we don't have enough
  // data for parsing then exit.
  if (!next_payload_known_ || buffer_.total_bytes() < next_payload_) {
    return nullptr;
  }

  CompoundBuffer* message_buffer = new CompoundBuffer();
  message_buffer->CopyFrom(buffer_, 0, next_payload_);
  message_buffer->Lock();
  buffer_.CropFront(next_payload_);
  next_payload_known_ = false;

  return message_buffer;
}

bool MessageDecoder::GetPayloadSize(int* size) {
  // The header has a size of 4 bytes.
  const int kHeaderSize = sizeof(int32_t);

  if (buffer_.total_bytes() < kHeaderSize) {
    return false;
  }

  CompoundBuffer header_buffer;
  char header[kHeaderSize];
  header_buffer.CopyFrom(buffer_, 0, kHeaderSize);
  header_buffer.CopyTo(header, kHeaderSize);
  *size = rtc::GetBE32(header);
  buffer_.CropFront(kHeaderSize);
  return true;
}

}  // namespace remoting::protocol
