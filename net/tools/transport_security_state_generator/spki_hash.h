// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_SPKI_HASH_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_SPKI_HASH_H_

#include <stdint.h>

#include <string_view>

namespace net::transport_security_state {

class SPKIHash {
 public:
  enum : size_t { kLength = 32 };

  SPKIHash();
  ~SPKIHash();

  // Initalizes a hash from the form sha256/<base64-hash-value>. The preloaded
  // SPKI hashes are SHA256. Other algorithms are not supported. Returns true
  // on success and copies the decoded bytes to |data_|. Returns false on
  // failure.
  bool FromString(std::string_view hash_string);

  // Calculates the SHA256 digest over |*input| and copies the result to
  // |data_|.
  void CalculateFromBytes(const uint8_t* input, size_t input_length);

  // Returns the size of the hash in bytes. Harcoded to 32 which is the length
  // of a SHA256 hash.
  size_t size() const { return kLength; }

  uint8_t* data() { return data_; }
  const uint8_t* data() const { return data_; }

 private:
  // The bytes of the hash. Current hashes are SHA256 and thus 32 bytes long.
  uint8_t data_[kLength];
};

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_SPKI_HASH_H_
