// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PINSET_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PINSET_H_

#include <string>
#include <vector>

namespace net::transport_security_state {

// A Pinset represents the data a website would send in a HPKP header. A pinset
// is given a name so that multiple entries in the preload list can reference
// the same pinset.
class Pinset {
 public:
  Pinset(std::string name, std::string report_uri);

  Pinset(const Pinset&) = delete;
  Pinset& operator=(const Pinset&) = delete;

  ~Pinset();

  const std::string& name() const { return name_; }
  const std::string& report_uri() const { return report_uri_; }

  const std::vector<std::string>& static_spki_hashes() const {
    return static_spki_hashes_;
  }
  const std::vector<std::string>& bad_static_spki_hashes() const {
    return bad_static_spki_hashes_;
  }

  // Register a good hash for this pinset. Hashes are referenced by a name, not
  // by the actual hash.
  void AddStaticSPKIHash(const std::string& hash_name);

  // Register a bad hash for this pinset. Hashes are referenced by a name, not
  // by the actual hash.
  void AddBadStaticSPKIHash(const std::string& hash_name);

 private:
  std::string name_;
  std::string report_uri_;

  // These vectors contain names rather than actual hashes.
  std::vector<std::string> static_spki_hashes_;
  std::vector<std::string> bad_static_spki_hashes_;
};

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PINSET_H_
