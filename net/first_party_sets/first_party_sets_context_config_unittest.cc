// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_context_config.h"

#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::Optional;

namespace net {

TEST(FirstPartySetsContextConfigTest, FindOverride_empty) {
  EXPECT_EQ(FirstPartySetsContextConfig().FindOverride(
                SchemefulSite(GURL("https://example.test"))),
            absl::nullopt);
}

TEST(FirstPartySetsContextConfigTest, FindOverride_irrelevant) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary, absl::nullopt);
  SchemefulSite foo(GURL("https://foo.test"));

  EXPECT_EQ(FirstPartySetsContextConfig({{example, entry}}).FindOverride(foo),
            absl::nullopt);
}

TEST(FirstPartySetsContextConfigTest, FindOverride_deletion) {
  SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(FirstPartySetsContextConfig({{example, absl::nullopt}})
                  .FindOverride(example),
              Optional(absl::nullopt));
}

TEST(FirstPartySetsContextConfigTest, FindOverride_modification) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary, absl::nullopt);

  EXPECT_THAT(
      FirstPartySetsContextConfig({{example, entry}}).FindOverride(example),
      Optional(Optional(entry)));
}

TEST(FirstPartySetsContextConfigTest, Contains) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite decoy(GURL("https://decoy.test"));

  FirstPartySetsContextConfig config({{example, absl::nullopt}});

  EXPECT_TRUE(config.Contains(example));
  EXPECT_FALSE(config.Contains(decoy));
}

TEST(FirstPartySetsContextConfigTest, ForEachCustomizationEntry_FullIteration) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite foo(GURL("https://foo.test"));

  FirstPartySetsContextConfig config(
      {{example, absl::nullopt}, {foo, absl::nullopt}});

  int count = 0;
  EXPECT_TRUE(config.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const absl::optional<FirstPartySetEntry>& entry) {
        ++count;
        return true;
      }));
  EXPECT_EQ(count, 2);
}

TEST(FirstPartySetsContextConfigTest, ForEachCustomizationEntry_EarlyReturn) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite foo(GURL("https://foo.test"));

  FirstPartySetsContextConfig config(
      {{example, absl::nullopt}, {foo, absl::nullopt}});

  int count = 0;
  EXPECT_FALSE(config.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const absl::optional<FirstPartySetEntry>& entry) {
        ++count;
        return count < 1;
      }));
  EXPECT_EQ(count, 1);
}

}  // namespace net
