// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/group_by_origin_key.h"

#include <vector>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class GroupByOriginKeyTest : public testing::Test {
 public:
 protected:
  url::Origin kOwner1 = url::Origin::Create(GURL("https://owner1.test"));
  url::Origin kJoin1 = url::Origin::Create(GURL("https://join1.test"));
  url::Origin kJoin2 = url::Origin::Create(GURL("https://join2.test"));
  url::Origin kProvider1 = url::Origin::Create(GURL("https://provider1.test"));
  url::Origin kProvider2 = url::Origin::Create(GURL("https://provider2.test"));
};

TEST_F(GroupByOriginKeyTest, Basic) {
  GroupByOriginKeyMapper mapper;

  {
    // With default execution mode, the group by origin key is just 0.
    StorageInterestGroup sig;
    sig.interest_group = blink::TestInterestGroupBuilder(kOwner1, "a").Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(0, mapper.LookupGroupByOriginId(
                     SingleStorageInterestGroup(std::move(sig)),
                     blink::InterestGroup::ExecutionMode::kCompatibilityMode));
  }

  {
    // Group-by-origin, first occurrence.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // Group-by-origin, reuse.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // Group-by-origin, first occurrence.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .Build();
    sig.joining_origin = kJoin2;

    EXPECT_EQ(2,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // Group-by-origin, reuse.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .Build();
    sig.joining_origin = kJoin2;

    EXPECT_EQ(2,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // Group-by-origin, reuse.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // We're still seeing 0 despite adding a bunch of stuff; it wasn't just
    // because it was first call.
    StorageInterestGroup sig;
    sig.interest_group = blink::TestInterestGroupBuilder(kOwner1, "a").Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(0, mapper.LookupGroupByOriginId(
                     SingleStorageInterestGroup(std::move(sig)),
                     blink::InterestGroup::ExecutionMode::kCompatibilityMode));
  }

  {
    // Frozen gets zeroes, too.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kFrozenContext)
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(0, mapper.LookupGroupByOriginId(
                     SingleStorageInterestGroup(std::move(sig)),
                     blink::InterestGroup::ExecutionMode::kFrozenContext));
  }
}

TEST_F(GroupByOriginKeyTest, BasicConfig) {
  GroupByOriginKeyMapper mapper;

  {
    // With grouped by origin mode, the group by origin key is just 1.
    StorageInterestGroup sig;
    sig.interest_group = blink::TestInterestGroupBuilder(kOwner1, "a").Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // With frozen context, the group by origin key is just 0.
    StorageInterestGroup sig;
    sig.interest_group = blink::TestInterestGroupBuilder(kOwner1, "a").Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(0, mapper.LookupGroupByOriginId(
                     SingleStorageInterestGroup(std::move(sig)),
                     blink::InterestGroup::ExecutionMode::kFrozenContext));
  }
  {
    // With compatibility, the group by origin key is just 0.
    StorageInterestGroup sig;
    sig.interest_group = blink::TestInterestGroupBuilder(kOwner1, "a").Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(0, mapper.LookupGroupByOriginId(
                     SingleStorageInterestGroup(std::move(sig)),
                     blink::InterestGroup::ExecutionMode::kCompatibilityMode));
  }
}

TEST_F(GroupByOriginKeyTest, BasicConfigDifferentOrigins) {
  GroupByOriginKeyMapper mapper;

  {
    // With grouped by origin mode, the group by origin key is just 1.
    StorageInterestGroup sig;
    sig.interest_group = blink::TestInterestGroupBuilder(kOwner1, "a").Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }
  {
    // With compatibility, the group by origin key is just 0.
    StorageInterestGroup sig;
    sig.interest_group = blink::TestInterestGroupBuilder(kOwner1, "a").Build();
    sig.joining_origin = kJoin2;

    EXPECT_EQ(2,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }
}

TEST_F(GroupByOriginKeyTest, Clickiness) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kFledgeClickiness);

  GroupByOriginKeyMapper mapper;

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider1}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider2}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(2,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider1, kProvider2}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(3,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // Order doesn't matter.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider2, kProvider1}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(3,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }
}

TEST_F(GroupByOriginKeyTest, ClickinessEmptyCanon) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kFledgeClickiness);

  GroupByOriginKeyMapper mapper;

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kOwner1}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders(std::nullopt)
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }
}

TEST_F(GroupByOriginKeyTest, ClickinessDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(blink::features::kFledgeClickiness);

  GroupByOriginKeyMapper mapper;

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider1}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider2}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider1, kProvider2}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }

  {
    // Order doesn't matter.
    StorageInterestGroup sig;
    sig.interest_group =
        blink::TestInterestGroupBuilder(kOwner1, "a")
            .SetExecutionMode(
                blink::InterestGroup::ExecutionMode::kGroupedByOriginMode)
            .SetViewAndClickCountsProviders({{kProvider2, kProvider1}})
            .Build();
    sig.joining_origin = kJoin1;

    EXPECT_EQ(1,
              mapper.LookupGroupByOriginId(
                  SingleStorageInterestGroup(std::move(sig)),
                  blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));
  }
}

}  // namespace content
