// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <stdlib.h>
#include <iostream>

#include "content/browser/first_party_sets/test/first_party_set_parser_map_fuzzer.pb.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

static const GURL kSiteTestCases[5] = {
    GURL("https://site-0.test"), GURL("https://site-1.test"),
    GURL("https://site-2.test"), GURL("https://site-3.test"),
    GURL("https://site-4.test")};

net::SchemefulSite GetSchemefulSite(const firstpartysets::proto::Site& site) {
  return net::SchemefulSite(kSiteTestCases[site.site_test_case_index()]);
}

FirstPartySetParser::SetsMap ConvertProtoToMap(
    const firstpartysets::proto::FirstPartySets& sets) {
  FirstPartySetParser::SetsMap map;
  for (const firstpartysets::proto::SitePair& item : sets.items()) {
    auto member_or_owner = GetSchemefulSite(item.member_or_owner());
    auto owner = GetSchemefulSite(item.owner());
    net::SiteType site_type = member_or_owner == owner
                                  ? net::SiteType::kPrimary
                                  : net::SiteType::kAssociated;
    absl::optional<net::FirstPartySetEntry::SiteIndex> site_index =
        site_type == net::SiteType::kPrimary
            ? absl::nullopt
            : absl::make_optional(
                  net::FirstPartySetEntry::SiteIndex(map.size()));
    map.emplace(member_or_owner,
                net::FirstPartySetEntry(owner, site_type, site_index));
  }
  return map;
}

bool AreEquivalent(FirstPartySetParser::SetsMap& native_input,
                   FirstPartySetParser::SetsMap& output) {
  if (native_input.empty() && output.empty())
    return true;

  auto is_owner_entry = [](const auto& pair) {
    return pair.first == pair.second.primary();
  };
  base::EraseIf(native_input, is_owner_entry);
  base::EraseIf(output, is_owner_entry);

  return native_input == output;
}

}  // namespace

DEFINE_PROTO_FUZZER(const firstpartysets::proto::FirstPartySets& input) {
  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << input.DebugString() << std::endl;

  FirstPartySetParser::SetsMap native_input = ConvertProtoToMap(input);

  FirstPartySetParser::SetsMap deserialized =
      FirstPartySetParser::DeserializeFirstPartySets(
          FirstPartySetParser::SerializeFirstPartySets(native_input));

  CHECK(deserialized.empty() || AreEquivalent(native_input, deserialized));
}

}  // namespace content