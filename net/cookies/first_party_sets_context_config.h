// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_
#define NET_COOKIES_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_

#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// This struct bundles together the customized settings to First-Party Sets
// info in the given network context.
class NET_EXPORT FirstPartySetsContextConfig {
 public:
  using OverrideSets =
      base::flat_map<SchemefulSite, absl::optional<FirstPartySetEntry>>;

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

}  // namespace net

#endif  // NET_COOKIES_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_