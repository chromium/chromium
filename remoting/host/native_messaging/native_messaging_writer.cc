// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/native_messaging/native_messaging_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace {

// 4-byte type used for the message header.
typedef uint32_t MessageLengthType;

// Limit the size of sent messages, since Chrome will not accept messages
// larger than 1MB, and this helps deal with the problem of integer overflow
// when passing sizes to net::FileStream APIs that take |int| parameters.
// This is defined as size_t (unsigned type) so it can be compared with the
// result of std::string::length() without compiler warnings.
const size_t kMaximumMessageSize = 1024 * 1024;

}  // namespace

namespace remoting {

NativeMessagingWriter::NativeMessagingWriter(base::File file)
    : write_stream_(std::move(file)), fail_(false) {}

NativeMessagingWriter::~NativeMessagingWriter() = default;

bool NativeMessagingWriter::WriteMessage(base::ValueView message) {
  if (fail_) {
    LOG(ERROR) << "Stream marked as corrupt.";
    return false;
  }

  std::string message_json;
  base::JSONWriter::Write(message, &message_json);

  CHECK_LE(message_json.length(), kMaximumMessageSize);

  // Cast from size_t to the proper header type, checking this won't overflow.
  MessageLengthType message_length =
      base::checked_cast<MessageLengthType>(message_json.length());

  if (!write_stream_.WriteAtCurrentPosAndCheck(
          base::byte_span_from_ref(message_length))) {
    LOG(ERROR) << "Failed to send message header";
    fail_ = true;
    return false;
  }

  if (!write_stream_.WriteAtCurrentPosAndCheck(
          base::as_byte_span(message_json))) {
    LOG(ERROR) << "Failed to send message body";
    fail_ = true;
    return false;
  }

  return true;
}

}  // namespace remoting
