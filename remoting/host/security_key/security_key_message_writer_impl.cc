// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_writer_impl.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/logging.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

SecurityKeyMessageWriterImpl::SecurityKeyMessageWriterImpl(
    base::File output_file)
    : output_stream_(std::move(output_file)) {}

SecurityKeyMessageWriterImpl::~SecurityKeyMessageWriterImpl() = default;

bool SecurityKeyMessageWriterImpl::WriteMessage(
    SecurityKeyMessageType message_type) {
  return WriteMessageWithPayload(message_type, std::string());
}

bool SecurityKeyMessageWriterImpl::WriteMessageWithPayload(
    SecurityKeyMessageType message_type,
    const std::string& message_payload) {
  if (write_failed_ || !output_stream_.IsValid()) {
    return false;
  }

  int message_payload_size_bytes = message_payload.size();
  uint32_t total_message_size_bytes =
      SecurityKeyMessage::kMessageTypeSizeBytes + message_payload_size_bytes;
  CHECK(SecurityKeyMessage::IsValidMessageSize(total_message_size_bytes));

  // First we send the message header which is the length of the message_type
  // and message_payload in bytes.
  if (!WriteBytesToOutput(base::byte_span_from_ref(total_message_size_bytes))) {
    LOG(ERROR) << "Failed to send message header.";
    return false;
  }

  // Next we send the message_type.
  if (!WriteBytesToOutput(base::byte_span_from_ref(message_type))) {
    LOG(ERROR) << "Failed to send message type.";
    return false;
  }

  // Lastly, send the message data if appropriate.
  if (!message_payload.empty()) {
    if (!WriteBytesToOutput(base::as_byte_span(message_payload))) {
      LOG(ERROR) << "Failed to send message payload.";
      return false;
    }
  }

  return true;
}

bool SecurityKeyMessageWriterImpl::WriteBytesToOutput(
    base::span<const uint8_t> message) {
  DCHECK(!message.empty());

  std::optional<size_t> result = output_stream_.WriteAtCurrentPos(message);
  if (!result.has_value() || result.value() != message.size()) {
    LOG(ERROR) << "Failed to write all bytes to output stream.  bytes written: "
               << (result.has_value() ? result.value() : 0) << ", file error: "
               << base::File::ErrorToString(output_stream_.error_details());
    write_failed_ = true;
    return false;
  }

  return true;
}

}  // namespace remoting
