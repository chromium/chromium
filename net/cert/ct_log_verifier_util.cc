// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier_util.h"

#include <array>

#include "base/containers/span.h"
#include "crypto/hash.h"

namespace net::ct::internal {

std::string HashNodes(const std::string& lh, const std::string& rh) {
  static constexpr std::array<uint8_t, 1> kTag = {0x01};

  crypto::hash::Hasher hash(crypto::hash::kSha256);
  hash.Update(kTag);
  hash.Update(lh);
  hash.Update(rh);

  std::string result(crypto::hash::kSha256Size, '\0');
  hash.Finish(base::as_writable_byte_span(result));
  return result;
}

}  // namespace net::ct::internal
