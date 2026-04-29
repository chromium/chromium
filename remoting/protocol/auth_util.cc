// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/auth_util.h"

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "crypto/hmac.h"

namespace remoting::protocol {

std::string GetSharedSecretHash(const std::string& tag,
                                const std::string& shared_secret) {
  // Note that the use of these parameters is backward from what you might
  // expect - a more usual construction would be to use the existing shared
  // secret as the key and the tag as the message, since here we're using the
  // hash as a key derivation function (but not using HKDF).
  base::span<const uint8_t> key = base::as_byte_span(tag);
  base::span<const uint8_t> message = base::as_byte_span(shared_secret);

  const auto hmac = crypto::hmac::SignSha256(key, message);
  return std::string(base::as_string_view(hmac));
}

}  // namespace remoting::protocol
