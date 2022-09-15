// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_

#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// This struct bundles together the customized settings to First-Party Sets
// info in the given network context.
class NET_EXPORT FirstPartySetsContextConfig {
 public:
  using OverrideSets =
      base::flat_map<SchemefulSite, absl::optional<FirstPartySetEntry>>;

  FirstPartySetsContextConfig();
  explicit FirstPartySetsContextConfig(OverrideSets customizations);

  FirstPartySetsContextConfig(FirstPartySetsContextConfig&& other);
  FirstPartySetsContextConfig& operator=(FirstPartySetsContextConfig&& other);

  ~FirstPartySetsContextConfig();

  FirstPartySetsContextConfig Clone() const;

  bool operator==(const FirstPartySetsContextConfig& other) const;

  const OverrideSets& customizations() const { return customizations_; }

 private:
  OverrideSets customizations_;
};

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_