// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_

#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// This struct bundles together the customized settings to First-Party Sets
// info in the given network context.
class FirstPartySetsContextConfig {
 public:
  using OverrideSets =
      base::flat_map<net::SchemefulSite, absl::optional<net::SchemefulSite>>;

  explicit FirstPartySetsContextConfig(bool enabled);

  FirstPartySetsContextConfig(const FirstPartySetsContextConfig& other);

  ~FirstPartySetsContextConfig();

  bool is_enabled() const { return enabled_; }

  void SetCustomizations(OverrideSets customizations);

  const OverrideSets& customizations() const { return customizations_; }

 private:
  bool enabled_ = true;

  OverrideSets customizations_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_