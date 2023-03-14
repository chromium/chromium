// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/local_set_declaration.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

absl::optional<FirstPartySetParser::SingleSet> CanonicalizeSet(
    const std::string& use_first_party_set_flag_value) {
  std::istringstream stream(use_first_party_set_flag_value);

  FirstPartySetParser::SetsAndAliases parsed =
      FirstPartySetParser::ParseSetsFromStream(stream, /*emit_errors=*/true,
                                               /*emit_metrics*/ false);

  FirstPartySetParser::SetsMap entries = std::move(parsed.first);
  FirstPartySetParser::Aliases aliases = std::move(parsed.second);

  if (entries.empty()) {
    return absl::nullopt;
  }

  const net::SchemefulSite primary = entries.begin()->second.primary();

  if (base::ranges::any_of(
          entries,
          [&primary](const FirstPartySetParser::SetsMap::value_type& pair) {
            return pair.second.primary() != primary;
          })) {
    // More than one set was provided. That is (currently) unsupported.
    LOG(ERROR) << "Ignoring use-first-party-set switch due to multiple set "
                  "declarations.";
    return absl::nullopt;
  }

  for (const auto& [alias, canonical] : aliases) {
    auto it = entries.find(canonical);
    CHECK(it != entries.end());
    bool inserted = entries.emplace(alias, it->second).second;
    CHECK(inserted);
  }

  return absl::make_optional(std::move(entries));
}

}  // namespace

LocalSetDeclaration::LocalSetDeclaration()
    : LocalSetDeclaration(absl::nullopt) {}

LocalSetDeclaration::LocalSetDeclaration(
    const std::string& use_first_party_set_flag_value)
    : LocalSetDeclaration(CanonicalizeSet(use_first_party_set_flag_value)) {}

LocalSetDeclaration::LocalSetDeclaration(
    absl::optional<FirstPartySetParser::SingleSet> parsed_set)
    : parsed_set_(std::move(parsed_set)) {}

LocalSetDeclaration::~LocalSetDeclaration() = default;

LocalSetDeclaration::LocalSetDeclaration(const LocalSetDeclaration&) = default;
LocalSetDeclaration& LocalSetDeclaration::operator=(
    const LocalSetDeclaration&) = default;

LocalSetDeclaration::LocalSetDeclaration(LocalSetDeclaration&&) = default;
LocalSetDeclaration& LocalSetDeclaration::operator=(LocalSetDeclaration&&) =
    default;

const FirstPartySetParser::SingleSet& LocalSetDeclaration::GetSet() const {
  CHECK(!empty());
  const FirstPartySetParser::SingleSet& set = parsed_set_.value();
  CHECK(!set.empty());
  return set;
}

}  // namespace content
