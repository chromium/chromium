// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/pinsets.h"

#include "base/strings/string_piece.h"
#include "net/tools/transport_security_state_generator/spki_hash.h"

namespace net::transport_security_state {

Pinsets::Pinsets() = default;

Pinsets::~Pinsets() = default;

void Pinsets::RegisterSPKIHash(base::StringPiece name, const SPKIHash& hash) {
  spki_hashes_.insert(
      std::pair<std::string, SPKIHash>(std::string(name), hash));
}

void Pinsets::RegisterPinset(std::unique_ptr<Pinset> pinset) {
  pinsets_.insert(std::pair<std::string, std::unique_ptr<Pinset>>(
      pinset->name(), std::move(pinset)));
}

}  // namespace net::transport_security_state
