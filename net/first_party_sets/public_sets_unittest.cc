// Copyright 2022 The Chromium Authors. All rights reserved.
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

using ::testing::Optional;

namespace net {

class PublicSetsTest : public ::testing::Test {
 public:
  PublicSetsTest() = default;

  FirstPartySetsContextConfig* config() { return &fps_context_config_; }

 private:
  FirstPartySetsContextConfig fps_context_config_;
};

TEST_F(PublicSetsTest, FindEntry_Nonexistent) {
  SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(PublicSets().FindEntry(example, config()), absl::nullopt);
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
                  .FindEntry(example, config()),
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
                  .FindEntry(wss_example, config()),
              Optional(entry));
}

TEST_F(PublicSetsTest, FindEntry_ExistsViaOverride) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  config()->SetCustomizations({{example, override_entry}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {})
                  .FindEntry(example, config()),
              Optional(override_entry));
}

TEST_F(PublicSetsTest, FindEntry_RemovedViaOverride) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);

  config()->SetCustomizations({{example, absl::nullopt}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {})
                  .FindEntry(example, config()),
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
                  .FindEntry(example_cctld, config()),
              Optional(entry));
}

TEST_F(PublicSetsTest, FindEntry_ExistsViaOverrideWithDecoyAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  config()->SetCustomizations({{example_cctld, override_entry}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {{example_cctld, example}})
                  .FindEntry(example_cctld, config()),
              Optional(override_entry));
}

TEST_F(PublicSetsTest, FindEntry_RemovedViaOverrideWithDecoyAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);

  config()->SetCustomizations({{example_cctld, absl::nullopt}});

  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {{example_cctld, example}})
                  .FindEntry(example_cctld, config()),
              absl::nullopt);
}

TEST_F(PublicSetsTest, FindEntry_AliasesIgnoredForConfig) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  config()->SetCustomizations({{example, override_entry}});

  // FindEntry should ignore aliases when using the customizations. Public
  // aliases only apply to sites in the public sets.
  EXPECT_THAT(PublicSets(
                  {
                      {example, public_entry},
                  },
                  {{example_cctld, example}})
                  .FindEntry(example_cctld, config()),
              public_entry);
}

}  // namespace net
