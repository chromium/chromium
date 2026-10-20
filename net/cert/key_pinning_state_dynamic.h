// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KEY_PINNING_STATE_DYNAMIC_H_
#define NET_CERT_KEY_PINNING_STATE_DYNAMIC_H_

#include "net/cert/key_pinning_state.h"

namespace net {

// Tracks which hosts have key pinning enabled using semantics like
// header-based PKP.
//
// Although header-based HPKP is dead, cronet allows the app to configure pins
// using similar semantics to HPKP. Unlike the "static" implementation, pins
// are configured on a per-host basis rather than using pinsets, and each
// record has its own expiration time.
class NET_EXPORT KeyPinningStateDynamic : public KeyPinningState {
 public:
  // TODO(crbug.com/497882860): refactor so that all pins are passed in to the
  // constructor and the rest of the methods are const.
  KeyPinningStateDynamic();

  // KeyPinningState implementation:
  bool GetPKPState(std::string_view host, PKPState* result) const override;

  // Adds explicitly-specified data as if it was processed from an HPKP header.
  // Note: dynamic PKP data is not persisted.
  void AddHPKP(std::string_view host,
               base::Time expiry,
               bool include_subdomains,
               const HashValueVector& hashes);

 private:
  typedef std::map<HashedHost, PKPState> PKPStateMap;

  ~KeyPinningStateDynamic() override;

  // The sets of hosts that have enabled key pinning. |domain| will always
  // be empty for a PKPState in this maps; the domain comes from the map keys
  // instead. In addition |HasPublicKeyPins| in the PKPState always returns
  // true.
  PKPStateMap enabled_pkp_hosts_;
};

}  // namespace net

#endif  // NET_CERT_KEY_PINNING_STATE_DYNAMIC_H_
