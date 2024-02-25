// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_OVERRIDES_POLICY_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_OVERRIDES_POLICY_H_

#include "content/common/content_export.h"
#include "net/first_party_sets/sets_mutation.h"

namespace content {

// This class holds the inputs/configuration associated with the
// RelatedWebsiteSetsOverrides/FirstPartySetsOverrides policy, after it has been
// parsed.
class CONTENT_EXPORT FirstPartySetsOverridesPolicy {
 public:
  explicit FirstPartySetsOverridesPolicy(net::SetsMutation mutation);

  FirstPartySetsOverridesPolicy(FirstPartySetsOverridesPolicy&&);
  FirstPartySetsOverridesPolicy& operator=(FirstPartySetsOverridesPolicy&&);

  ~FirstPartySetsOverridesPolicy();

  bool operator==(const FirstPartySetsOverridesPolicy& other) const;

  const net::SetsMutation& mutation() const { return mutation_; }
  net::SetsMutation& mutation() { return mutation_; }

 private:
  net::SetsMutation mutation_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_OVERRIDES_POLICY_H_
