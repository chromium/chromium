// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_VALIDATOR_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_VALIDATOR_H_

#include <map>

#include "net/base/net_export.h"

namespace net {

class SchemefulSite;

// The helper class to check the validity of the Related Website Sets
// (First-Party Sets). E.g. Check whether the sets contain any singleton or
// orphan site.
//
// A singleton: some primary site that names a set with no non-primary sites.
//
// An orphan: some non-primary site whose primary has no entry in any set.
class NET_EXPORT FirstPartySetsValidator {
 public:
  FirstPartySetsValidator();
  ~FirstPartySetsValidator();

  FirstPartySetsValidator(FirstPartySetsValidator&&) = delete;
  FirstPartySetsValidator& operator=(FirstPartySetsValidator&&) = delete;

  // Updates the internal state of the validator with `site` and its `primary`.
  // This method should be called once for each site in each set.
  void Update(const SchemefulSite& site, const SchemefulSite& primary);

  // Returns true iff all the calls to `Update` thus far describe a valid set.
  [[nodiscard]] bool IsValid() const;

  // Returns true iff `primary` has valid primary state for the RWSs.
  [[nodiscard]] bool IsSitePrimaryValid(const SchemefulSite& primary) const;

 private:
  struct PrimarySiteState {
    // A primary site is a singleton iff it is never used as the primary in some
    // other site's entry.
    bool has_nonself_entry = false;
    // A primary site induces orphaned non-primary sites iff it is used as the
    // primary site in some other site's entry, but it has no entry itself.
    bool has_self_entry = false;
  };

  std::map<SchemefulSite, PrimarySiteState> primary_states_;
};

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_VALIDATOR_H_
