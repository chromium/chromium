// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_context_config.h"

#include <optional>

#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

MATCHER_P(OverridesTo, entry, "") {
  return !arg.IsDeletion() &&
         testing::ExplainMatchResult(entry, arg.GetEntry(), result_listener);
}

namespace net {

TEST(FirstPartySetsContextConfigTest, FindOverride_empty) {
  EXPECT_EQ(FirstPartySetsContextConfig().FindOverride(
                SchemefulSite(GURL("https://example.test"))),
            std::nullopt);
}

TEST(FirstPartySetsContextConfigTest, FindOverride_irrelevant) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary);
  SchemefulSite foo(GURL("https://foo.test"));

  EXPECT_EQ(FirstPartySetsContextConfig::Create(
                {{example, FirstPartySetEntryOverride(entry)}})
                .value()
                .FindOverride(foo),
            std::nullopt);
}

TEST(FirstPartySetsContextConfigTest, FindOverride_deletion) {
  SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(FirstPartySetsContextConfig::Create(
                  {{example, FirstPartySetEntryOverride()}})
                  .value()
                  .FindOverride(example),
              Optional(FirstPartySetEntryOverride()));
}

TEST(FirstPartySetsContextConfigTest, FindOverride_modification) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary);

  EXPECT_THAT(FirstPartySetsContextConfig::Create(
                  {{example, FirstPartySetEntryOverride(entry)}})
                  .value()
                  .FindOverride(example),
              Optional(OverridesTo(entry)));
}

TEST(FirstPartySetsContextConfigTest, Contains) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite decoy(GURL("https://decoy.test"));

  FirstPartySetsContextConfig config =
      FirstPartySetsContextConfig::Create(
          {{example, FirstPartySetEntryOverride()}})
          .value();

  EXPECT_TRUE(config.Contains(example));
  EXPECT_FALSE(config.Contains(decoy));
}

TEST(FirstPartySetsContextConfigTest, ForEachCustomizationEntry_FullIteration) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite foo(GURL("https://foo.test"));

  FirstPartySetsContextConfig config =
      FirstPartySetsContextConfig::Create(
          {{example, FirstPartySetEntryOverride()},
           {foo, FirstPartySetEntryOverride()}})
          .value();

  int count = 0;
  EXPECT_TRUE(config.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const FirstPartySetEntryOverride& override) {
        ++count;
        return true;
      }));
  EXPECT_EQ(count, 2);
}

TEST(FirstPartySetsContextConfigTest, ForEachCustomizationEntry_EarlyReturn) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite foo(GURL("https://foo.test"));

  FirstPartySetsContextConfig config =
      FirstPartySetsContextConfig::Create(
          {{example, FirstPartySetEntryOverride()},
           {foo, FirstPartySetEntryOverride()}})
          .value();

  int count = 0;
  EXPECT_FALSE(config.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const FirstPartySetEntryOverride& override) {
        ++count;
        return count < 1;
      }));
  EXPECT_EQ(count, 1);
}

TEST(FirstPartySetsContextConfigTest, Clone) {
  EXPECT_EQ(FirstPartySetsContextConfig().Clone(),
            FirstPartySetsContextConfig());

  const SchemefulSite example(GURL("https://example.test"));
  const SchemefulSite foo(GURL("https://foo.test"));
  const SchemefulSite foo_alias(GURL("https://foo.test2"));

  const FirstPartySetEntry primary_entry(example, SiteType::kPrimary);
  const FirstPartySetEntry associated_entry(example, SiteType::kAssociated);

  const FirstPartySetsContextConfig config =
      FirstPartySetsContextConfig::Create(
          /*customizations=*/
          {
              {example, FirstPartySetEntryOverride(primary_entry)},
              {foo, FirstPartySetEntryOverride(associated_entry)},
              {foo_alias, FirstPartySetEntryOverride(associated_entry)},
          },
          /*aliases=*/
          {
              {foo_alias, foo},
          })
          .value();
  EXPECT_EQ(config.Clone(), config);
}

TEST(FirstPartySetsContextConfigTest, ForEachAlias) {
  const SchemefulSite example(GURL("https://example.test"));
  const SchemefulSite foo(GURL("https://foo.test"));
  const SchemefulSite foo_alias(GURL("https://foo.test2"));
  const SchemefulSite bar(GURL("https://bar.test"));
  const SchemefulSite bar_alias(GURL("https://bar.test2"));

  const FirstPartySetEntry primary_entry(example, SiteType::kPrimary);
  const FirstPartySetEntry associated_entry(example, SiteType::kAssociated);
  const FirstPartySetEntry service_entry(example, SiteType::kService);

  const FirstPartySetsContextConfig config =
      FirstPartySetsContextConfig::Create(
          /*customizations=*/
          {
              {example, FirstPartySetEntryOverride(primary_entry)},
              {foo, FirstPartySetEntryOverride(associated_entry)},
              {foo_alias, FirstPartySetEntryOverride(associated_entry)},
              {bar, FirstPartySetEntryOverride(service_entry)},
              {bar_alias, FirstPartySetEntryOverride(service_entry)},
          },
          /*aliases=*/
          {
              {foo_alias, foo},
              {bar_alias, bar},
          })
          .value();

  std::vector<std::pair<SchemefulSite, SchemefulSite>> observed;
  config.ForEachAlias(
      [&](const SchemefulSite& alias, const SchemefulSite& canonical) {
        observed.emplace_back(alias, canonical);
      });
  EXPECT_THAT(observed,
              UnorderedElementsAre(Pair(foo_alias, foo), Pair(bar_alias, bar)));
}

}  // namespace net
