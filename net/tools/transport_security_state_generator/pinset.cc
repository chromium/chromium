// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/pinset.h"

namespace net::transport_security_state {

Pinset::Pinset(std::string name, std::string report_uri)
    : name_(name), report_uri_(report_uri) {}

Pinset::~Pinset() = default;

void Pinset::AddStaticSPKIHash(const std::string& hash_name) {
  static_spki_hashes_.push_back(hash_name);
}

void Pinset::AddBadStaticSPKIHash(const std::string& hash_name) {
  bad_static_spki_hashes_.push_back(hash_name);
}

}  // namespace net::transport_security_state
