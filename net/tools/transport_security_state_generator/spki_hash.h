// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_SPKI_HASH_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_SPKI_HASH_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string_view>

#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/sha2.h"

namespace net::transport_security_state {

class SPKIHash {
 public:
  SPKIHash();
  ~SPKIHash();

  // Initalizes a hash from the form sha256/<base64-hash-value>. The preloaded
  // SPKI hashes are SHA256. Other algorithms are not supported. Returns true
  // on success and copies the decoded bytes to |data_|. Returns false on
  // failure.
  bool FromString(std::string_view hash_string);

  // Calculates the SHA256 digest over |*input| and copies the result to
  // |data_|.
  void CalculateFromBytes(base::span<const uint8_t> bytes);

  // Returns the size of the hash in bytes.
  size_t size() const { return data_.size(); }

  base::span<uint8_t> span() { return data_; }
  base::span<const uint8_t> span() const { return data_; }

 private:
  std::array<uint8_t, SHA256_DIGEST_LENGTH> data_;
};

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_SPKI_HASH_H_
