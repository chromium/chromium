// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_MESSAGE_WRITER_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_MESSAGE_WRITER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/security_key/security_key_message_writer.h"

namespace remoting {

// Simulates the SecurityKeyMessageWriter and provides access to data
// members for testing.
class FakeSecurityKeyMessageWriter : public SecurityKeyMessageWriter {
 public:
  explicit FakeSecurityKeyMessageWriter(
      const base::RepeatingClosure& write_callback);

  FakeSecurityKeyMessageWriter(const FakeSecurityKeyMessageWriter&) = delete;
  FakeSecurityKeyMessageWriter& operator=(const FakeSecurityKeyMessageWriter&) =
      delete;

  ~FakeSecurityKeyMessageWriter() override;

  // SecurityKeyMessageWriter interface.
  bool WriteMessage(SecurityKeyMessageType message_type) override;
  bool WriteMessageWithPayload(SecurityKeyMessageType message_type,
                               const std::string& message_payload) override;

  base::WeakPtr<FakeSecurityKeyMessageWriter> AsWeakPtr();

  SecurityKeyMessageType last_message_type() { return last_message_type_; }

  const std::string& last_message_payload() { return last_message_payload_; }

  void set_write_request_succeeded(bool should_succeed) {
    write_request_succeeded_ = should_succeed;
  }

 private:
  // Tracks the last message_type value written.
  SecurityKeyMessageType last_message_type_ = SecurityKeyMessageType::INVALID;

  // Tracks the last message_payload value written.
  std::string last_message_payload_;

  // This value is returned by the WriteMessage* functions above.
  bool write_request_succeeded_ = true;

  // Signaled whenever a write is requested.
  base::RepeatingClosure write_callback_;

  base::WeakPtrFactory<FakeSecurityKeyMessageWriter> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_MESSAGE_WRITER_H_
