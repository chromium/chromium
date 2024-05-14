// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_set_entry.h"

#include <tuple>

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

FirstPartySetEntry::SiteIndex::SiteIndex() = default;

FirstPartySetEntry::SiteIndex::SiteIndex(uint32_t value) : value_(value) {}

bool FirstPartySetEntry::SiteIndex::operator==(const SiteIndex& other) const =
    default;

FirstPartySetEntry::FirstPartySetEntry() = default;

FirstPartySetEntry::FirstPartySetEntry(
    SchemefulSite primary,
    SiteType site_type,
    std::optional<FirstPartySetEntry::SiteIndex> site_index)
    : primary_(primary), site_type_(site_type), site_index_(site_index) {
  switch (site_type_) {
    case SiteType::kPrimary:
    case SiteType::kService:
      CHECK(!site_index_.has_value());
      break;
    case SiteType::kAssociated:
      break;
  }
}

FirstPartySetEntry::FirstPartySetEntry(SchemefulSite primary,
                                       SiteType site_type,
                                       uint32_t site_index)
    : FirstPartySetEntry(
          primary,
          site_type,
          std::make_optional(FirstPartySetEntry::SiteIndex(site_index))) {}

FirstPartySetEntry::FirstPartySetEntry(const FirstPartySetEntry&) = default;
FirstPartySetEntry& FirstPartySetEntry::operator=(const FirstPartySetEntry&) =
    default;
FirstPartySetEntry::FirstPartySetEntry(FirstPartySetEntry&&) = default;
FirstPartySetEntry& FirstPartySetEntry::operator=(FirstPartySetEntry&&) =
    default;

FirstPartySetEntry::~FirstPartySetEntry() = default;

bool FirstPartySetEntry::operator==(const FirstPartySetEntry& other) const =
    default;

bool FirstPartySetEntry::operator!=(const FirstPartySetEntry& other) const =
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
      NOTREACHED_IN_MIGRATION() << "Unknown SiteType: " << value;
  }
  return std::nullopt;
}

std::string FirstPartySetEntry::GetDebugString() const {
  return base::StrCat({"{primary: ", primary_.GetDebugString(),
                       ", site_type: ", SiteTypeToString(site_type_), "}"});
}

std::ostream& operator<<(std::ostream& os,
                         const FirstPartySetEntry::SiteIndex& index) {
  os << index.value();
  return os;
}

std::ostream& operator<<(std::ostream& os, const FirstPartySetEntry& entry) {
  os << "{" << entry.primary() << ", " << static_cast<int>(entry.site_type())
     << ", ";
  if (entry.site_index().has_value()) {
    os << entry.site_index().value();
  } else {
    os << "{}";
  }
  os << "}";
  return os;
}

}  // namespace net
