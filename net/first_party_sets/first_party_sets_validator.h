// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_VALIDATOR_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_VALIDATOR_H_

#include <map>

#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"

namespace net {

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

  FirstPartySetsValidator(FirstPartySetsValidator&&);
  FirstPartySetsValidator& operator=(FirstPartySetsValidator&&);
  FirstPartySetsValidator(const FirstPartySetsValidator&) = delete;
  FirstPartySetsValidator& operator=(const FirstPartySetsValidator&) = delete;

  // Updates the internal state of the validator with `site` and its `primary`.
  // This method should be called once for each site in each set.
  void Update(const SchemefulSite& site, const SchemefulSite& primary);

  // Returns true iff all the calls to `Update` thus far describe a valid set.
  [[nodiscard]] bool IsValid() const;

  // Returns true iff `primary` has valid primary state for the RWSs.
  [[nodiscard]] bool IsSitePrimaryValid(const SchemefulSite& primary) const;

 private:
  struct PrimarySiteState {
    bool IsValid() const;

    // A primary site is a singleton iff it is never used as the primary in some
    // other site's entry.
    bool has_nonself_entry = false;
    // A primary site induces orphaned non-primary sites iff it is used as the
    // primary site in some other site's entry, but it has no entry itself.
    bool has_self_entry = false;
    // True iff none of the sites in this primary's set appear in any other set.
    bool is_disjoint = true;
  };

  struct SiteState {
    SchemefulSite first_seen_primary;
  };

  // Tracks validity states for each primary site.
  std::map<SchemefulSite, PrimarySiteState> primary_states_;

  // Tracks metadata for each site.
  std::map<SchemefulSite, SiteState> site_metadatas_;
};

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_VALIDATOR_H_
