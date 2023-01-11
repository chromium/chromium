// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_MESSAGE_READER_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_MESSAGE_READER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/security_key/security_key_message_reader.h"

namespace remoting {

// Simulates the SecurityKeyMessageReader and provides access to data
// members for testing.
class FakeSecurityKeyMessageReader : public SecurityKeyMessageReader {
 public:
  FakeSecurityKeyMessageReader();

  FakeSecurityKeyMessageReader(const FakeSecurityKeyMessageReader&) = delete;
  FakeSecurityKeyMessageReader& operator=(const FakeSecurityKeyMessageReader&) =
      delete;

  ~FakeSecurityKeyMessageReader() override;

  // SecurityKeyMessageReader interface.
  void Start(const SecurityKeyMessageCallback& message_callback,
             base::OnceClosure error_callback) override;

  base::WeakPtr<FakeSecurityKeyMessageReader> AsWeakPtr();

  const SecurityKeyMessageCallback& message_callback() {
    return message_callback_;
  }

 private:
  // Caller-supplied message and error callbacks.
  SecurityKeyMessageCallback message_callback_;
  base::OnceClosure error_callback_;

  base::WeakPtrFactory<FakeSecurityKeyMessageReader> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_MESSAGE_READER_H_
