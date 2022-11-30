// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_WRITER_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_WRITER_H_

#include <string>

#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

// Used for sending security key messages using a file handle.
class SecurityKeyMessageWriter {
 public:
  virtual ~SecurityKeyMessageWriter() {}

  // Writes a security key message w/o a payload to |output_stream_|.
  virtual bool WriteMessage(SecurityKeyMessageType message_type) = 0;

  // Writes a security key message with a payload to |output_stream_|.
  virtual bool WriteMessageWithPayload(SecurityKeyMessageType message_type,
                                       const std::string& message_payload) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_WRITER_H_
