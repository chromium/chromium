// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier_util.h"

#include <memory>

#include "base/strings/string_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace net::ct::internal {

std::string HashNodes(const std::string& lh, const std::string& rh) {
  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hash->Update("\01", 1);
  hash->Update(lh.data(), lh.size());
  hash->Update(rh.data(), rh.size());

  std::string result(crypto::kSHA256Length, '\0');
  hash->Finish(result.data(), result.size());
  return result;
}

}  // namespace net::ct::internal
