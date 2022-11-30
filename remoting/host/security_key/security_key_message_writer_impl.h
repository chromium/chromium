// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_WRITER_IMPL_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_WRITER_IMPL_H_

#include <string>

#include "base/files/file.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/security_key/security_key_message_writer.h"

namespace remoting {

// Used for sending security key messages using a file handle.
class SecurityKeyMessageWriterImpl : public SecurityKeyMessageWriter {
 public:
  explicit SecurityKeyMessageWriterImpl(base::File output_file);

  SecurityKeyMessageWriterImpl(const SecurityKeyMessageWriterImpl&) = delete;
  SecurityKeyMessageWriterImpl& operator=(const SecurityKeyMessageWriterImpl&) =
      delete;

  ~SecurityKeyMessageWriterImpl() override;

 private:
  // SecurityKeyMessageWriter interface.
  bool WriteMessage(SecurityKeyMessageType message_type) override;
  bool WriteMessageWithPayload(SecurityKeyMessageType message_type,
                               const std::string& message_payload) override;

  // Writes |bytes_to_write| bytes from |message| to |output_stream_|.
  bool WriteBytesToOutput(const char* message, int bytes_to_write);

  base::File output_stream_;
  bool write_failed_ = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_WRITER_IMPL_H_
