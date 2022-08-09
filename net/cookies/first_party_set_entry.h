// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_FIRST_PARTY_SET_ENTRY_H_
#define NET_COOKIES_FIRST_PARTY_SET_ENTRY_H_

#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"

namespace net {

enum class SiteType {
  // The First-Party Set declaration listed this site as the "primary" site for
  // the set.
  kPrimary,
  // The First-Party Set declaration listed this site as an associated site in
  // the set.
  kAssociated,
};

// This class bundles together metadata associated with an entry in a
// First-Party Set.
class NET_EXPORT FirstPartySetEntry {
 public:
  FirstPartySetEntry();

  // `primary` is the primary site in the First-Party Set associated with this
  // entry.
  FirstPartySetEntry(SchemefulSite primary, SiteType site_type);

  FirstPartySetEntry(const FirstPartySetEntry&);
  FirstPartySetEntry& operator=(const FirstPartySetEntry&);
  FirstPartySetEntry(FirstPartySetEntry&&);
  FirstPartySetEntry& operator=(FirstPartySetEntry&&);

  ~FirstPartySetEntry();

  bool operator==(const FirstPartySetEntry& other) const;
  bool operator!=(const FirstPartySetEntry& other) const;

  const SchemefulSite& primary() const { return primary_; }

  SiteType site_type() const { return site_type_; }

 private:
  SchemefulSite primary_;
  SiteType site_type_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const FirstPartySetEntry& fpse);

}  // namespace net

#endif  // NET_COOKIES_FIRST_PARTY_SET_ENTRY_H_
