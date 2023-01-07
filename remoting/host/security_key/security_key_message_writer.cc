// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_writer.h"

#include <cstdint>
#include <string>
#include <utility>

#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

SecurityKeyMessageWriter::SecurityKeyMessageWriter(base::File output_file)
    : output_stream_(std::move(output_file)) {}

SecurityKeyMessageWriter::~SecurityKeyMessageWriter() {}

bool SecurityKeyMessageWriter::WriteMessage(
    SecurityKeyMessageType message_type) {
  return WriteMessageWithPayload(message_type, std::string());
}

bool SecurityKeyMessageWriter::WriteMessageWithPayload(
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
  if (!WriteBytesToOutput(reinterpret_cast<char*>(&total_message_size_bytes),
                          SecurityKeyMessage::kHeaderSizeBytes)) {
    LOG(ERROR) << "Failed to send message header.";
    return false;
  }

  // Next we send the message_type.
  if (!WriteBytesToOutput(reinterpret_cast<char*>(&message_type),
                          SecurityKeyMessage::kMessageTypeSizeBytes)) {
    LOG(ERROR) << "Failed to send message type.";
    return false;
  }

  // Lastly, send the message data if appropriate.
  if (!message_payload.empty()) {
    if (!WriteBytesToOutput(message_payload.data(),
                            message_payload_size_bytes)) {
      LOG(ERROR) << "Failed to send message payload.";
      return false;
    }
  }

  return true;
}

bool SecurityKeyMessageWriter::WriteBytesToOutput(const char* message,
                                                  int bytes_to_write) {
  DCHECK(message);
  DCHECK_GT(bytes_to_write, 0);

  int result = output_stream_.WriteAtCurrentPos(message, bytes_to_write);
  if (result != bytes_to_write) {
    LOG(ERROR) << "Failed to write all bytes to output stream.  bytes written: "
               << result << ", file error: "
               << base::File::ErrorToString(output_stream_.error_details());
    write_failed_ = true;
    return false;
  }

  return true;
}

}  // namespace remoting
