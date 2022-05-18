// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_

namespace network {

// This struct bundles together the customized settings to First-Party Sets
// info in the given network context.
class FirstPartySetsContextConfig {
 public:
  FirstPartySetsContextConfig() = default;
  explicit FirstPartySetsContextConfig(bool enabled);

  bool is_enabled() const { return enabled_; }

 private:
  bool enabled_ = true;
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_