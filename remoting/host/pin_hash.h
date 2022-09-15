// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PIN_HASH_H_
#define REMOTING_HOST_PIN_HASH_H_

#include <string>

namespace remoting {

// Creates a Me2Me shared-secret hash, consisting of the hash method, and the
// hashed host ID and PIN.
std::string MakeHostPinHash(const std::string& host_id, const std::string& pin);

// Parse string representation of a shared secret hash. The value can be either
// "plain:<pin_in_base64>" or "hmac:<pin_hmac_in_base64>". In the first case the
// returned value is automatically hashed. False is returned if |value| is in
// invalid format.
bool ParsePinHashFromConfig(const std::string& value,
                            const std::string& host_id,
                            std::string* pin_hash_out);

// Extracts the hash function from the given hash, uses it to calculate the
// hash of the given host ID and PIN, and compares that hash to the given hash.
// Returns true if the calculated and given hashes are equal.
bool VerifyHostPinHash(const std::string& hash,
                       const std::string& host_id,
                       const std::string& pin);

}  // namespace remoting

#endif  // REMOTING_HOST_PIN_HASH_H_
