// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_security_key_message_reader.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

FakeSecurityKeyMessageReader::FakeSecurityKeyMessageReader() {}

FakeSecurityKeyMessageReader::~FakeSecurityKeyMessageReader() = default;

base::WeakPtr<FakeSecurityKeyMessageReader>
FakeSecurityKeyMessageReader::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FakeSecurityKeyMessageReader::Start(
    const SecurityKeyMessageCallback& message_callback,
    const base::Closure& error_callback) {
  message_callback_ = message_callback;
  error_callback_ = error_callback;
}

}  // namespace remoting
