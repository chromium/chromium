// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/message_serialization.h"

#include <stdint.h>

#include "net/base/io_buffer.h"
#include "third_party/webrtc/rtc_base/byte_order.h"

namespace remoting::protocol {

scoped_refptr<net::IOBufferWithSize> SerializeAndFrameMessage(
    const google::protobuf::MessageLite& msg) {
  // Create a buffer with 4 extra bytes. This is used as prefix to write an
  // int32_t of the serialized message size for framing.
  const int kExtraBytes = sizeof(int32_t);
  int size = msg.ByteSize() + kExtraBytes;
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(size);
  rtc::SetBE32(buffer->data(), msg.GetCachedSize());
  msg.SerializeWithCachedSizesToArray(buffer->bytes() + kExtraBytes);
  return buffer;
}

}  // namespace remoting::protocol
