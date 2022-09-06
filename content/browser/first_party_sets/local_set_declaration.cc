// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/local_set_declaration.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

absl::optional<std::pair<net::SchemefulSite, FirstPartySetParser::SingleSet>>
CanonicalizeSet(base::StringPiece use_first_party_set_flag_value) {
  const std::vector<std::string> origins =
      base::SplitString(use_first_party_set_flag_value, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (origins.empty())
    return absl::nullopt;

  const absl::optional<net::SchemefulSite> maybe_primary =
      content::FirstPartySetParser::CanonicalizeRegisteredDomain(
          origins[0], true /* emit_errors */);
  if (!maybe_primary.has_value()) {
    LOG(ERROR) << "First-Party Set primary is not valid; aborting.";
    return absl::nullopt;
  }

  const net::SchemefulSite& primary = *maybe_primary;
  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> sites(
      {{primary, net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                         absl::nullopt)}});
  base::flat_set<net::SchemefulSite> associated_sites;
  for (auto it = origins.begin() + 1; it != origins.end(); ++it) {
    const absl::optional<net::SchemefulSite> maybe_associated_site =
        content::FirstPartySetParser::CanonicalizeRegisteredDomain(
            *it, true /* emit_errors */);
    if (maybe_associated_site.has_value() && maybe_associated_site != primary &&
        !base::Contains(associated_sites, *maybe_associated_site)) {
      sites.emplace_back(
          *maybe_associated_site,
          net::FirstPartySetEntry(primary, net::SiteType::kAssociated,
                                  associated_sites.size()));
      associated_sites.insert(*maybe_associated_site);
    }
  }

  if (sites.size() < 2) {
    // We're guaranteed at least one site (the primary), but there needs to be
    // at least one other site as well.
    LOG(ERROR)
        << "The First-Party Set must contain more than one site; aborting.";
    return absl::nullopt;
  }

  return absl::make_optional(
      std::make_pair(std::move(primary), std::move(sites)));
}

}  // namespace

LocalSetDeclaration::LocalSetDeclaration()
    : LocalSetDeclaration(absl::nullopt) {}

LocalSetDeclaration::LocalSetDeclaration(
    base::StringPiece use_first_party_set_flag_value)
    : LocalSetDeclaration(CanonicalizeSet(use_first_party_set_flag_value)) {}

LocalSetDeclaration::LocalSetDeclaration(
    absl::optional<std::pair<net::SchemefulSite,
                             FirstPartySetParser::SingleSet>> parsed_set)
    : parsed_set_(std::move(parsed_set)) {}

LocalSetDeclaration::~LocalSetDeclaration() = default;

LocalSetDeclaration::LocalSetDeclaration(const LocalSetDeclaration&) = default;
LocalSetDeclaration& LocalSetDeclaration::operator=(
    const LocalSetDeclaration&) = default;

LocalSetDeclaration::LocalSetDeclaration(LocalSetDeclaration&&) = default;
LocalSetDeclaration& LocalSetDeclaration::operator=(LocalSetDeclaration&&) =
    default;

net::SchemefulSite LocalSetDeclaration::GetPrimary() const {
  DCHECK(!empty());
  return parsed_set_->first;
}

FirstPartySetParser::SingleSet LocalSetDeclaration::GetSet() const {
  DCHECK(!empty());
  DCHECK(!parsed_set_->second.empty());
  return parsed_set_->second;
}

}  // namespace content
