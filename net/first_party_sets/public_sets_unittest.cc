// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/public_sets.h"

#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace net {

const SchemefulSite kPrimary(GURL("https://primary.test"));
const SchemefulSite kPrimary2(GURL("https://primary2.test"));
const SchemefulSite kPrimary3(GURL("https://primary3.test"));
const SchemefulSite kAssociated1(GURL("https://associated1.test"));
const SchemefulSite kAssociated1Cctld(GURL("https://associated1.cctld"));
const SchemefulSite kAssociated1Cctld2(GURL("https://associated1.cctld2"));
const SchemefulSite kAssociated2(GURL("https://associated2.test"));
const SchemefulSite kAssociated3(GURL("https://associated3.test"));
const SchemefulSite kAssociated4(GURL("https://associated4.test"));
const SchemefulSite kService(GURL("https://service.test"));

class PublicSetsTest : public ::testing::Test {
 public:
  PublicSetsTest() = default;
};

TEST_F(PublicSetsTest, FindEntry_Nonexistent) {
  SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(PublicSets().FindEntry(example, /*config=*/nullptr),
              absl::nullopt);
}

TEST_F(PublicSetsTest, FindEntry_Exists) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite decoy_site(GURL("https://decoy.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry decoy_entry(example, SiteType::kAssociated, 1);

  EXPECT_THAT(PublicSets(
                  {
                      {example, entry},
                      {decoy_site, decoy_entry},
                  },
                  {})
                  .FindEntry(example, /*config=*/nullptr),
              Optional(entry));
}

TEST_F(PublicSetsTest, FindEntry_ExistsWhenNormalized) {
  SchemefulSite https_example(GURL("https://example.test"));
  SchemefulSite wss_example(GURL("wss://example.test"));
  FirstPartySetEntry entry(https_example, SiteType::kPrimary, absl::nullopt);

  EXPECT_THAT(PublicSets(
                  {
                      {https_example, entry},
                  },
                  {})
                  .FindEntry(wss_example, /*config=*/nullptr),
              Optional(entry));
}

TEST_F(PublicSetsTest, FindEntry_ExistsViaOverride) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  FirstPartySetsContextConfig config({{example, override_entry}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {})
                  .FindEntry(example, &config),
              Optional(override_entry));
}

TEST_F(PublicSetsTest, FindEntry_RemovedViaOverride) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);

  FirstPartySetsContextConfig config({{example, absl::nullopt}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {})
                  .FindEntry(example, &config),
              absl::nullopt);
}

TEST_F(PublicSetsTest, FindEntry_ExistsViaAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry entry(example, SiteType::kPrimary, absl::nullopt);

  EXPECT_THAT(PublicSets(
                  {
                      {example, entry},
                  },
                  {{example_cctld, example}})
                  .FindEntry(example_cctld, /*config=*/nullptr),
              Optional(entry));
}

TEST_F(PublicSetsTest, FindEntry_ExistsViaOverrideWithDecoyAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  FirstPartySetsContextConfig config({{example_cctld, override_entry}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {{example_cctld, example}})
                  .FindEntry(example_cctld, &config),
              Optional(override_entry));
}

TEST_F(PublicSetsTest, FindEntry_RemovedViaOverrideWithDecoyAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);

  FirstPartySetsContextConfig config({{example_cctld, absl::nullopt}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {{example_cctld, example}})
                  .FindEntry(example_cctld, &config),
              absl::nullopt);
}

TEST_F(PublicSetsTest, FindEntry_AliasesIgnoredForConfig) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  FirstPartySetsContextConfig config({{example, override_entry}});

  // FindEntry should ignore aliases when using the customizations. Public
  // aliases only apply to sites in the public sets.
  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {{example_cctld, example}})
                  .FindEntry(example_cctld, &config),
              public_entry);
}

class PopulatedPublicSetsTest : public PublicSetsTest {
 public:
  PopulatedPublicSetsTest()
      : public_sets_(
            {
                {kPrimary, FirstPartySetEntry(kPrimary,
                                              SiteType::kPrimary,
                                              absl::nullopt)},
                {kAssociated1,
                 FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
                {kAssociated2,
                 FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
                {kService, FirstPartySetEntry(kPrimary,
                                              SiteType::kService,
                                              absl::nullopt)},
                {kPrimary2, FirstPartySetEntry(kPrimary2,
                                               SiteType::kPrimary,
                                               absl::nullopt)},
                {kAssociated3,
                 FirstPartySetEntry(kPrimary2, SiteType::kAssociated, 0)},
            },
            {
                {kAssociated1Cctld, kAssociated1},
            }) {}

  PublicSets& public_sets() { return public_sets_; }

 private:
  PublicSets public_sets_;
};

TEST_F(PopulatedPublicSetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesPrimaryPrimary) {
  // kPrimary overlaps as primary of both sets, so the existing set should be
  // wiped out.
  public_sets().ApplyManuallySpecifiedSet(
      kPrimary,
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated4,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      {});

  EXPECT_THAT(
      public_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kAssociated1Cctld,
          },
          /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kAssociated4,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedPublicSetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesPrimaryNonprimary) {
  // kPrimary overlaps as a primary of the public set and non-primary of the CLI
  // set, so the existing set should be wiped out.
  public_sets().ApplyManuallySpecifiedSet(
      kPrimary3,
      {
          {kPrimary3,
           FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
          {kPrimary, FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
      },
      {});

  EXPECT_THAT(
      public_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kPrimary3,
              kAssociated1Cctld,
          },
          /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(kPrimary3, FirstPartySetEntry(kPrimary3, SiteType::kPrimary,
                                             absl::nullopt)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedPublicSetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesNonprimaryPrimary) {
  // kAssociated1 overlaps as a non-primary of the public set and primary of the
  // CLI set, so the CLI set should steal it and wipe out its alias, but
  // otherwise leave the set intact.
  public_sets().ApplyManuallySpecifiedSet(
      kAssociated1,
      {
          {kAssociated1,
           FirstPartySetEntry(kAssociated1, SiteType::kPrimary, absl::nullopt)},
          {kAssociated4,
           FirstPartySetEntry(kAssociated1, SiteType::kAssociated, 0)},
      },
      {});

  EXPECT_THAT(
      public_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kPrimary3,
              kAssociated1Cctld,
          },
          /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)),
          Pair(kService,
               FirstPartySetEntry(kPrimary, SiteType::kService, absl::nullopt)),
          Pair(kAssociated1,
               FirstPartySetEntry(kAssociated1, SiteType::kPrimary,
                                  absl::nullopt)),
          Pair(kAssociated4,
               FirstPartySetEntry(kAssociated1, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedPublicSetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesNonprimaryNonprimary) {
  // kAssociated1 overlaps as a non-primary of the public set and non-primary of
  // the CLI set, so the CLI set should steal it and wipe out its alias.
  public_sets().ApplyManuallySpecifiedSet(
      kPrimary3,
      {
          {kPrimary3,
           FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
      },
      {});

  EXPECT_THAT(
      public_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kPrimary3,
              kAssociated1Cctld,
          },
          /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)),
          Pair(kService,
               FirstPartySetEntry(kPrimary, SiteType::kService, absl::nullopt)),
          Pair(kPrimary3, FirstPartySetEntry(kPrimary3, SiteType::kPrimary,
                                             absl::nullopt)),
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedPublicSetsTest,
       ApplyManuallySpecifiedSet_PrunesInducedSingletons) {
  // Steal kAssociated3, so that kPrimary2 becomes a singleton, and verify that
  // kPrimary2 is no longer considered in a set.
  public_sets().ApplyManuallySpecifiedSet(
      kPrimary3,
      {
          {kPrimary3,
           FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
          {kAssociated3,
           FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
      },
      {});

  EXPECT_THAT(public_sets().FindEntries({kPrimary2}, /*config=*/nullptr),
              IsEmpty());
}

TEST_F(PopulatedPublicSetsTest, ApplyManuallySpecifiedSet_RespectsManualAlias) {
  // Both the public sets and the locally-defined set define an alias for
  // kAssociated1, but both define a different set for that site too.  Only the
  // locally-defined alias should be observable.
  public_sets().ApplyManuallySpecifiedSet(
      kPrimary3,
      {
          {kPrimary3,
           FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
      },
      {{kAssociated1Cctld2, kAssociated1}});

  EXPECT_THAT(
      public_sets().FindEntries(
          {
              kAssociated1,
              kAssociated1Cctld,
              kAssociated1Cctld2,
          },
          /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)),
          Pair(kAssociated1Cctld2,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0))));
}

}  // namespace net
