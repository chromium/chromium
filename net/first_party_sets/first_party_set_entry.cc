// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_set_entry.h"

#include <tuple>
#include <utility>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "net/base/schemeful_site.h"

namespace net {

namespace {

std::string SiteTypeToString(SiteType site_type) {
  switch (site_type) {
    case SiteType::kPrimary:
      return "kPrimary";
    case SiteType::kAssociated:
      return "kAssociated";
    case SiteType::kService:
      return "kService";
  }
}

}  // namespace

FirstPartySetEntry::FirstPartySetEntry() = default;

FirstPartySetEntry::FirstPartySetEntry(SchemefulSite primary,
                                       SiteType site_type)
    : primary_(std::move(primary)), site_type_(site_type) {}

FirstPartySetEntry::FirstPartySetEntry(const FirstPartySetEntry&) = default;
FirstPartySetEntry& FirstPartySetEntry::operator=(const FirstPartySetEntry&) =
    default;
FirstPartySetEntry::FirstPartySetEntry(FirstPartySetEntry&&) = default;
FirstPartySetEntry& FirstPartySetEntry::operator=(FirstPartySetEntry&&) =
    default;

FirstPartySetEntry::~FirstPartySetEntry() = default;

bool FirstPartySetEntry::operator==(const FirstPartySetEntry& other) const =
    default;

// static
std::optional<net::SiteType> FirstPartySetEntry::DeserializeSiteType(
    int value) {
  switch (value) {
    case static_cast<int>(net::SiteType::kPrimary):
      return net::SiteType::kPrimary;
    case static_cast<int>(net::SiteType::kAssociated):
      return net::SiteType::kAssociated;
    case static_cast<int>(net::SiteType::kService):
      return net::SiteType::kService;
    default:
      NOTREACHED() << "Unknown SiteType: " << value;
  }
}

std::string FirstPartySetEntry::GetDebugString() const {
  return base::StrCat({"{primary: ", primary_.GetDebugString(),
                       ", site_type: ", SiteTypeToString(site_type_), "}"});
}

std::ostream& operator<<(std::ostream& os, const FirstPartySetEntry& entry) {
  os << "{" << entry.primary() << ", " << static_cast<int>(entry.site_type())
     << "}";
  return os;
}

}  // namespace net
