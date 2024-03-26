// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PINSETS_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PINSETS_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "net/tools/transport_security_state_generator/cert_util.h"
#include "net/tools/transport_security_state_generator/pinset.h"
#include "net/tools/transport_security_state_generator/spki_hash.h"

namespace net::transport_security_state {

// Contains SPKIHashes and their names. The names are used to reference
// the hashes from Pinset's.
using SPKIHashMap = std::map<std::string, SPKIHash>;
using PinsetMap = std::map<std::string, std::unique_ptr<Pinset>>;

class Pinsets {
 public:
  Pinsets();

  Pinsets(const Pinsets&) = delete;
  Pinsets& operator=(const Pinsets&) = delete;

  ~Pinsets();

  void RegisterSPKIHash(std::string_view name, const SPKIHash& hash);
  void RegisterPinset(std::unique_ptr<Pinset> set);

  size_t size() const { return pinsets_.size(); }
  size_t spki_size() const { return spki_hashes_.size(); }

  const SPKIHashMap& spki_hashes() const { return spki_hashes_; }
  const PinsetMap& pinsets() const { return pinsets_; }

 private:
  // Contains all SPKI hashes found in the input pins file.
  SPKIHashMap spki_hashes_;

  // Contains all pinsets in the input JSON file.
  PinsetMap pinsets_;
};

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PINSETS_H_
