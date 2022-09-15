// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pin_hash.h"

#include "base/base64.h"
#include "base/logging.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"

namespace remoting {

bool ParsePinHashFromConfig(const std::string& value,
                            const std::string& host_id,
                            std::string* pin_hash_out) {
  size_t separator = value.find(':');
  if (separator == std::string::npos)
    return false;

  if (!base::Base64Decode(value.substr(separator + 1), pin_hash_out))
    return false;

  std::string function_name = value.substr(0, separator);
  if (function_name == "plain") {
    *pin_hash_out = protocol::GetSharedSecretHash(host_id, *pin_hash_out);
    return true;
  } else if (function_name == "hmac") {
    return true;
  }

  pin_hash_out->clear();
  return false;
}

std::string MakeHostPinHash(const std::string& host_id,
                            const std::string& pin) {
  std::string hash = protocol::GetSharedSecretHash(host_id, pin);
  std::string hash_base64;
  base::Base64Encode(hash, &hash_base64);
  return "hmac:" + hash_base64;
}

bool VerifyHostPinHash(const std::string& hash,
                       const std::string& host_id,
                       const std::string& pin) {
  std::string hash_parsed;
  if (!ParsePinHashFromConfig(hash, host_id, &hash_parsed)) {
    LOG(FATAL) << "Failed to parse PIN hash.";
    return false;
  }
  std::string hash_calculated = protocol::GetSharedSecretHash(host_id, pin);
  return hash_calculated == hash_parsed;
}

}  // namespace remoting
