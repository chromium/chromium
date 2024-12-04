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
  FirstPartySetEntry entry(example, SiteType::kPrimary, std::nullopt);
  SchemefulSite foo(GURL("https://foo.test"));

  EXPECT_EQ(FirstPartySetsContextConfig(
                {{example, FirstPartySetEntryOverride(entry)}})
                .FindOverride(foo),
            std::nullopt);
}

TEST(FirstPartySetsContextConfigTest, FindOverride_deletion) {
  SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(
      FirstPartySetsContextConfig({{example, FirstPartySetEntryOverride()}})
          .FindOverride(example),
      Optional(FirstPartySetEntryOverride()));
}

TEST(FirstPartySetsContextConfigTest, FindOverride_modification) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary, std::nullopt);

  EXPECT_THAT(FirstPartySetsContextConfig(
                  {{example, FirstPartySetEntryOverride(entry)}})
                  .FindOverride(example),
              Optional(OverridesTo(entry)));
}

TEST(FirstPartySetsContextConfigTest, Contains) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite decoy(GURL("https://decoy.test"));

  FirstPartySetsContextConfig config({{example, FirstPartySetEntryOverride()}});

  EXPECT_TRUE(config.Contains(example));
  EXPECT_FALSE(config.Contains(decoy));
}

TEST(FirstPartySetsContextConfigTest, ForEachCustomizationEntry_FullIteration) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite foo(GURL("https://foo.test"));

  FirstPartySetsContextConfig config({{example, FirstPartySetEntryOverride()},
                                      {foo, FirstPartySetEntryOverride()}});

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

  FirstPartySetsContextConfig config({{example, FirstPartySetEntryOverride()},
                                      {foo, FirstPartySetEntryOverride()}});

  int count = 0;
  EXPECT_FALSE(config.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const FirstPartySetEntryOverride& override) {
        ++count;
        return count < 1;
      }));
  EXPECT_EQ(count, 1);
}

}  // namespace net
