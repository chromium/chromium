// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/global_first_party_sets.h"

#include <optional>

#include "base/containers/flat_map.h"
#include "base/version.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/sets_mutation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace net {

namespace {

const base::Version kVersion("1.2.3");
const SchemefulSite kPrimary(GURL("https://primary.test"));
const SchemefulSite kPrimaryCctld(GURL("https://primary.ccltd"));
const SchemefulSite kPrimary2(GURL("https://primary2.test"));
const SchemefulSite kPrimary3(GURL("https://primary3.test"));
const SchemefulSite kAssociated1(GURL("https://associated1.test"));
const SchemefulSite kAssociated1Cctld(GURL("https://associated1.cctld"));
const SchemefulSite kAssociated1Cctld2(GURL("https://associated1.cctld2"));
const SchemefulSite kAssociated2(GURL("https://associated2.test"));
const SchemefulSite kAssociated3(GURL("https://associated3.test"));
const SchemefulSite kAssociated4(GURL("https://associated4.test"));
const SchemefulSite kAssociated5(GURL("https://associated5.test"));
const SchemefulSite kService(GURL("https://service.test"));

base::flat_map<SchemefulSite, FirstPartySetEntry> CollectEffectiveSetEntries(
    const GlobalFirstPartySets& sets) {
  base::flat_map<SchemefulSite, FirstPartySetEntry> got;
  sets.ForEachEffectiveSetEntry(
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) {
        EXPECT_FALSE(got.contains(site));
        got[site] = entry;
        return true;
      });

  // Consistency check: verify that all of the returned entries are what we'd
  // get if we called FindEntry directly.
  for (const auto& [site, entry] : got) {
    EXPECT_EQ(sets.FindEntry(site).value(), entry);
  }
  return got;
}

base::flat_map<SchemefulSite, FirstPartySetEntry> FindEntries(
    const GlobalFirstPartySets& sets,
    const base::flat_set<net::SchemefulSite>& sites) {
  base::flat_map<SchemefulSite, FirstPartySetEntry> got =
      CollectEffectiveSetEntries(sets);
  base::EraseIf(got,
                [&](const auto& pair) { return !sites.contains(pair.first); });
  return got;
}

}  // namespace

class GlobalFirstPartySetsTest : public ::testing::Test {
 public:
  GlobalFirstPartySetsTest() = default;
};

TEST_F(GlobalFirstPartySetsTest, CtorSkipsInvalidVersion) {
  GlobalFirstPartySets sets = GlobalFirstPartySets::CreateForTesting(
      base::Version(), /*entries=*/
      {
          {kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary)},
          {kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated)},
      },
      /*aliases=*/{});

  EXPECT_THAT(FindEntries(sets, {kPrimary, kAssociated1}), IsEmpty());
}

TEST_F(GlobalFirstPartySetsTest, Clone) {
  base::Version version("1.2.3.4.5");
  const SchemefulSite example(GURL("https://example.test"));
  const SchemefulSite example_cctld(GURL("https://example.cctld"));
  const SchemefulSite member1(GURL("https://member1.test"));
  const FirstPartySetEntry entry(example, SiteType::kPrimary);
  const FirstPartySetEntry member1_entry(example, SiteType::kAssociated);

  const SchemefulSite foo(GURL("https://foo.test"));
  const SchemefulSite member2(GURL("https://member2.test"));
  const FirstPartySetEntry foo_entry(foo, SiteType::kPrimary);
  const FirstPartySetEntry member2_entry(foo, SiteType::kAssociated);

  GlobalFirstPartySets sets = GlobalFirstPartySets::CreateForTesting(
      version, /*entries=*/{{example, entry}, {member1, member1_entry}},
      /*aliases=*/{{example_cctld, example}});

  EXPECT_EQ(sets, sets.Clone());
}

TEST_F(GlobalFirstPartySetsTest, Ctor_PrimaryWithAlias_Valid) {
  GlobalFirstPartySets global_sets = GlobalFirstPartySets::CreateForTesting(
      kVersion,
      /*entries=*/
      {
          {kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary)},
      },
      /*aliases=*/
      {
          {kPrimaryCctld, kPrimary},
      });

  EXPECT_THAT(
      CollectEffectiveSetEntries(global_sets),
      UnorderedElementsAre(
          Pair(kPrimaryCctld, FirstPartySetEntry(kPrimary, SiteType::kPrimary)),
          Pair(kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary))));
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_Nonexistent) {
  SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(GlobalFirstPartySets().FindEntry(example), std::nullopt);
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_Exists) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite decoy_site(GURL("https://decoy.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary);
  FirstPartySetEntry decoy_entry(example, SiteType::kAssociated);

  EXPECT_THAT(
      GlobalFirstPartySets::CreateForTesting(kVersion,
                                             {
                                                 {example, entry},
                                                 {decoy_site, decoy_entry},
                                             },
                                             {})
          .FindEntry(example),
      Optional(entry));
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_NoNormalization) {
  SchemefulSite https_example(GURL("https://example.test"));
  SchemefulSite associated(GURL("https://associated.test"));
  SchemefulSite wss_example(GURL("wss://example.test"));
  FirstPartySetEntry entry(https_example, SiteType::kPrimary);
  FirstPartySetEntry assoc_entry(https_example, SiteType::kAssociated);

  EXPECT_THAT(
      GlobalFirstPartySets::CreateForTesting(kVersion,
                                             {
                                                 {https_example, entry},
                                                 {associated, assoc_entry},
                                             },
                                             {})
          .FindEntry(wss_example),
      std::nullopt);
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_ExistsViaAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry entry(example, SiteType::kPrimary);

  EXPECT_THAT(GlobalFirstPartySets::CreateForTesting(kVersion,
                                                     {
                                                         {example, entry},
                                                     },
                                                     {{example_cctld, example}})
                  .FindEntry(example_cctld),
              Optional(entry));
}

TEST_F(GlobalFirstPartySetsTest, Empty_Empty) {
  EXPECT_TRUE(GlobalFirstPartySets().empty());
}

TEST_F(GlobalFirstPartySetsTest, Empty_NonemptyEntries) {
  EXPECT_FALSE(
      GlobalFirstPartySets::CreateForTesting(
          kVersion,
          {
              {kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary)},
              {kAssociated4,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated)},
          },
          {})
          .empty());
}

class PopulatedGlobalFirstPartySetsTest : public GlobalFirstPartySetsTest {
 public:
  PopulatedGlobalFirstPartySetsTest()
      : global_sets_(GlobalFirstPartySets::CreateForTesting(
            kVersion,
            {
                {kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary)},
                {kAssociated1,
                 FirstPartySetEntry(kPrimary, SiteType::kAssociated)},
                {kAssociated2,
                 FirstPartySetEntry(kPrimary, SiteType::kAssociated)},
                {kService, FirstPartySetEntry(kPrimary, SiteType::kService)},
                {kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary)},
                {kAssociated3,
                 FirstPartySetEntry(kPrimary2, SiteType::kAssociated)},
            },
            {
                {kAssociated1Cctld, kAssociated1},
            })) {}

  GlobalFirstPartySets& global_sets() { return global_sets_; }

 private:
  GlobalFirstPartySets global_sets_;
};

TEST_F(PopulatedGlobalFirstPartySetsTest, ForEachPublicSetEntry_FullIteration) {
  int count = 0;
  EXPECT_TRUE(global_sets().ForEachPublicSetEntry(
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) {
        ++count;
        return true;
      }));
  EXPECT_EQ(count, 7);
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ForEachPublicSetEntry_EarlyReturn) {
  int count = 0;
  EXPECT_FALSE(global_sets().ForEachPublicSetEntry(
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) {
        ++count;
        return count < 4;
      }));
  EXPECT_EQ(count, 4);
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ForEachEffectiveSetEntry_PublicSetsOnly_FullIteration) {
  EXPECT_THAT(
      CollectEffectiveSetEntries(global_sets()),
      UnorderedElementsAre(
          Pair(kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated)),
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated)),
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated)),
          Pair(kAssociated3,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated)),
          Pair(kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary)),
          Pair(kService, FirstPartySetEntry(kPrimary, SiteType::kService))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata) {
  SchemefulSite nonmember(GURL("https://nonmember.test"));
  FirstPartySetEntry primary_entry(kPrimary, SiteType::kPrimary);
  FirstPartySetEntry associated_entry(kPrimary, SiteType::kAssociated);

  // Works as usual for sites that are in First-Party sets.
  EXPECT_EQ(global_sets().ComputeMetadata(kAssociated1, &kAssociated1),
            FirstPartySetMetadata(associated_entry, associated_entry));
  EXPECT_EQ(global_sets().ComputeMetadata(kPrimary, &kAssociated1),
            FirstPartySetMetadata(primary_entry, associated_entry));
  EXPECT_EQ(global_sets().ComputeMetadata(kAssociated1, &kPrimary),
            FirstPartySetMetadata(associated_entry, primary_entry));

  EXPECT_EQ(global_sets().ComputeMetadata(nonmember, &kAssociated1),
            FirstPartySetMetadata(std::nullopt, associated_entry));
  EXPECT_EQ(global_sets().ComputeMetadata(kAssociated1, &nonmember),
            FirstPartySetMetadata(associated_entry, std::nullopt));

  EXPECT_EQ(global_sets().ComputeMetadata(nonmember, &nonmember),
            FirstPartySetMetadata(std::nullopt, std::nullopt));
}

}  // namespace net
