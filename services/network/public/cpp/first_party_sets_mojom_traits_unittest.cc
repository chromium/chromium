// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/first_party_sets_mojom_traits.h"

#include "base/containers/flat_map.h"
#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

using testing::Key;
using testing::UnorderedElementsAre;

TEST(FirstPartySetsTraitsTest, Roundtrips_SiteIndex) {
  net::FirstPartySetEntry::SiteIndex original(1337);
  net::FirstPartySetEntry::SiteIndex round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SiteIndex>(
      original, round_tripped));

  EXPECT_EQ(original, round_tripped);
}

TEST(FirstPartySetsTraitsTest, Roundtrips_SiteType) {
  for (net::SiteType site_type : {
           net::SiteType::kPrimary,
           net::SiteType::kAssociated,
           net::SiteType::kService,
       }) {
    net::SiteType roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SiteType>(
        site_type, roundtrip));
    EXPECT_EQ(site_type, roundtrip);
  }
}

TEST(FirstPartySetsTraitsTest, Roundtrips_FirstPartySetEntry) {
  net::SchemefulSite primary(GURL("https://primary.test"));

  net::FirstPartySetEntry original(primary, net::SiteType::kAssociated, 1);
  net::FirstPartySetEntry round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FirstPartySetEntry>(
      original, round_tripped));

  EXPECT_EQ(original, round_tripped);
  EXPECT_EQ(round_tripped.primary(), primary);
}

TEST(FirstPartySetsTraitsTest, Roundtrips_FirstPartySetMetadata) {
  net::SchemefulSite frame_owner(GURL("https://frame.test"));
  net::SchemefulSite top_frame_owner(GURL("https://top_frame.test"));

  net::FirstPartySetEntry frame_entry(frame_owner, net::SiteType::kAssociated,
                                      1);
  net::FirstPartySetEntry top_frame_entry(top_frame_owner,
                                          net::SiteType::kAssociated, 2);

  auto make_metadata = [&]() {
    // Use non-default values to ensure serialization/deserialization works
    // properly.
    return net::FirstPartySetMetadata(&frame_entry, &top_frame_entry);
  };

  net::FirstPartySetMetadata original = make_metadata();
  net::FirstPartySetMetadata round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::FirstPartySetMetadata>(original, round_tripped));

  EXPECT_EQ(round_tripped.frame_entry(), frame_entry);
  EXPECT_EQ(round_tripped.top_frame_entry(), top_frame_entry);

  EXPECT_EQ(round_tripped, make_metadata());
}

TEST(FirstPartySetsTraitsTest, RoundTrips_GlobalFirstPartySets) {
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite b_cctld(GURL("https://b.cctld"));
  net::SchemefulSite c(GURL("https://c.test"));
  net::SchemefulSite c_cctld(GURL("https://c.cctld"));

  net::GlobalFirstPartySets original(
      base::Version("1.2.3"),
      /*entries=*/
      {
          {a,
           net::FirstPartySetEntry(a, net::SiteType::kPrimary, std::nullopt)},
          {b, net::FirstPartySetEntry(a, net::SiteType::kAssociated, 0)},
          {c,
           net::FirstPartySetEntry(a, net::SiteType::kService, std::nullopt)},
      },
      /*aliases=*/{{c_cctld, c}});

  original.ApplyManuallySpecifiedSet(net::LocalSetDeclaration(
      /*set_entries=*/{{a, net::FirstPartySetEntry(a, net::SiteType::kPrimary,
                                                   std::nullopt)},
                       {b, net::FirstPartySetEntry(
                               a, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{{b_cctld, b}}));

  net::GlobalFirstPartySets round_tripped;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::GlobalFirstPartySets>(
          original, round_tripped));

  EXPECT_EQ(original, round_tripped);
  EXPECT_FALSE(round_tripped.empty());
}

TEST(FirstPartySetsTraitsTest, GlobalFirstPartySets_InvalidVersion) {
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite b_cctld(GURL("https://b.cctld"));
  net::SchemefulSite c(GURL("https://c.test"));
  net::SchemefulSite c_cctld(GURL("https://c.cctld"));

  net::GlobalFirstPartySets original(
      base::Version(),
      /*entries=*/
      {
          {a,
           net::FirstPartySetEntry(a, net::SiteType::kPrimary, std::nullopt)},
          {b, net::FirstPartySetEntry(a, net::SiteType::kAssociated, 0)},
          {c,
           net::FirstPartySetEntry(a, net::SiteType::kService, std::nullopt)},
      },
      /*aliases=*/{{c_cctld, c}});

  original.ApplyManuallySpecifiedSet(net::LocalSetDeclaration(
      /*set_entries=*/{{a, net::FirstPartySetEntry(a, net::SiteType::kPrimary,
                                                   std::nullopt)},
                       {b, net::FirstPartySetEntry(
                               a, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{{b_cctld, b}}));

  net::GlobalFirstPartySets round_tripped;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::GlobalFirstPartySets>(
          original, round_tripped));

  EXPECT_FALSE(round_tripped.empty());

  // base::Version::operator== crashes for invalid versions, so we don't check
  // equality of `round_tripped` and `original` that way. However, we can verify
  // that the original entries and alias are not present in `round_tripped`:
  EXPECT_THAT(round_tripped.FindEntries({a, b, b_cctld, c, c_cctld},
                                        net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(Key(a), Key(b), Key(b_cctld)));
}

TEST(FirstPartySetsTraitsTest, RoundTrips_FirstPartySetsContextConfig) {
  net::SchemefulSite a(GURL("https://a.test"));
  net::SchemefulSite b(GURL("https://b.test"));
  net::SchemefulSite c(GURL("https://c.test"));

  const net::FirstPartySetsContextConfig original({
      {a, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
              a, net::SiteType::kPrimary, std::nullopt))},
      {b, net::FirstPartySetEntryOverride(
              net::FirstPartySetEntry(a, net::SiteType::kAssociated, 0))},
      {c, net::FirstPartySetEntryOverride()},
  });

  net::FirstPartySetsContextConfig round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::FirstPartySetsContextConfig>(original,
                                                           round_tripped));

  EXPECT_EQ(original, round_tripped);
}

TEST(FirstPartySetsTraitsTest, RoundTrips_FirstPartySetsCacheFilter) {
  net::SchemefulSite a(GURL("https://a.test"));
  int64_t kClearAtRunId = 2;
  int64_t kBrowserRunId = 3;

  const net::FirstPartySetsCacheFilter original({{a, kClearAtRunId}},
                                                kBrowserRunId);

  net::FirstPartySetsCacheFilter round_tripped;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<
          network::mojom::FirstPartySetsCacheFilter>(original, round_tripped));

  EXPECT_EQ(original, round_tripped);
}

}  // namespace
}  // namespace network
