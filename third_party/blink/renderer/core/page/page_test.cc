// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/mojom/partitioned_popins/partitioned_popin_params.mojom.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/scoped_browsing_context_group_pauser.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(PageTest, CreateOrdinaryBrowsingContextGroup) {
  test::TaskEnvironment task_environment;
  EmptyChromeClient* client = MakeGarbageCollected<EmptyChromeClient>();
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();
  auto bcg_info = BrowsingContextGroupInfo::CreateUnique();

  Page* page =
      Page::CreateOrdinary(*client, /*opener=*/nullptr, *scheduler, bcg_info,
                           /*color_provider_colors=*/nullptr,
                           /*partitioned_popin_params=*/nullptr);

  EXPECT_EQ(page->BrowsingContextGroupToken(),
            bcg_info.browsing_context_group_token);
  EXPECT_EQ(page->CoopRelatedGroupToken(), bcg_info.coop_related_group_token);
}

TEST(PageTest, CreateNonOrdinaryBrowsingContextGroup) {
  test::TaskEnvironment task_environment;
  EmptyChromeClient* client = MakeGarbageCollected<EmptyChromeClient>();
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();

  Page* page = Page::CreateNonOrdinary(*client, *scheduler,
                                       /*color_provider_colors=*/nullptr);

  EXPECT_FALSE(page->BrowsingContextGroupToken().is_empty());
  EXPECT_FALSE(page->CoopRelatedGroupToken().is_empty());

  EXPECT_NE(page->BrowsingContextGroupToken(), page->CoopRelatedGroupToken());
}

TEST(PageTest, BrowsingContextGroupUpdate) {
  test::TaskEnvironment task_environment;
  EmptyChromeClient* client = MakeGarbageCollected<EmptyChromeClient>();
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();
  auto initial_bcg_info = BrowsingContextGroupInfo::CreateUnique();

  Page* page = Page::CreateOrdinary(*client, /*opener=*/nullptr, *scheduler,
                                    initial_bcg_info,
                                    /*color_provider_colors=*/nullptr,
                                    /*partitioned_popin_params=*/nullptr);

  EXPECT_EQ(page->BrowsingContextGroupToken(),
            initial_bcg_info.browsing_context_group_token);
  EXPECT_EQ(page->CoopRelatedGroupToken(),
            initial_bcg_info.coop_related_group_token);

  auto updated_bcg_info = BrowsingContextGroupInfo::CreateUnique();
  page->UpdateBrowsingContextGroup(updated_bcg_info);

  EXPECT_EQ(page->BrowsingContextGroupToken(),
            updated_bcg_info.browsing_context_group_token);
  EXPECT_EQ(page->CoopRelatedGroupToken(),
            updated_bcg_info.coop_related_group_token);
}

TEST(PageTest, BrowsingContextGroupUpdateWithPauser) {
  test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPausePagesPerBrowsingContextGroup);

  EmptyChromeClient* client = MakeGarbageCollected<EmptyChromeClient>();
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();

  auto group_a = BrowsingContextGroupInfo::CreateUnique();

  Page* page1 =
      Page::CreateOrdinary(*client, /*opener=*/nullptr, *scheduler, group_a,
                           /*color_provider_colors=*/nullptr,
                           /*partitioned_popin_params=*/nullptr);

  auto pauser_for_group_a =
      std::make_unique<ScopedBrowsingContextGroupPauser>(*page1);
  ASSERT_TRUE(page1->Paused());

  auto group_b = BrowsingContextGroupInfo::CreateUnique();
  page1->UpdateBrowsingContextGroup(group_b);
  ASSERT_FALSE(page1->Paused());

  Page* page2 =
      Page::CreateOrdinary(*client, /*opener=*/nullptr, *scheduler, group_b,
                           /*color_provider_colors=*/nullptr,
                           /*partitioned_popin_params=*/nullptr);
  ASSERT_FALSE(page2->Paused());

  page2->UpdateBrowsingContextGroup(group_a);
  ASSERT_TRUE(page2->Paused());

  pauser_for_group_a.reset();
  ASSERT_FALSE(page2->Paused());
}

TEST(PageTest, CreateOrdinaryColorProviders) {
  test::TaskEnvironment task_environment;
  EmptyChromeClient* client = MakeGarbageCollected<EmptyChromeClient>();
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();
  auto bcg_info = BrowsingContextGroupInfo::CreateUnique();
  auto color_provider_colors = ColorProviderColorMaps::CreateDefault();

  Page* page = Page::CreateOrdinary(*client, /*opener=*/nullptr, *scheduler,
                                    bcg_info, &color_provider_colors,
                                    /*partitioned_popin_params=*/nullptr);

  const ui::ColorProvider* light_color_provider =
      page->GetColorProviderForPainting(
          /*color_scheme=*/mojom::blink::ColorScheme::kLight,
          /*in_forced_colors=*/false);
  const ui::ColorProvider* dark_color_provider =
      page->GetColorProviderForPainting(
          /*color_scheme=*/mojom::blink::ColorScheme::kDark,
          /*in_forced_colors=*/false);
  const ui::ColorProvider* forced_colors_color_provider =
      page->GetColorProviderForPainting(
          /*color_scheme=*/mojom::blink::ColorScheme::kLight,
          /*in_forced_colors=*/true);

  // All color provider instances should be non-null.
  ASSERT_TRUE(light_color_provider);
  ASSERT_TRUE(dark_color_provider);
  ASSERT_TRUE(forced_colors_color_provider);
}

TEST(PageTest, CreateNonOrdinaryColorProviders) {
  test::TaskEnvironment task_environment;
  EmptyChromeClient* client = MakeGarbageCollected<EmptyChromeClient>();
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();

  Page* page = Page::CreateNonOrdinary(*client, *scheduler,
                                       /*color_provider_colors=*/nullptr);

  const ui::ColorProvider* light_color_provider =
      page->GetColorProviderForPainting(
          /*color_scheme=*/mojom::blink::ColorScheme::kLight,
          /*in_forced_colors=*/false);
  const ui::ColorProvider* dark_color_provider =
      page->GetColorProviderForPainting(
          /*color_scheme=*/mojom::blink::ColorScheme::kDark,
          /*in_forced_colors=*/false);
  const ui::ColorProvider* forced_colors_color_provider =
      page->GetColorProviderForPainting(
          /*color_scheme=*/mojom::blink::ColorScheme::kLight,
          /*in_forced_colors=*/true);

  // All color provider instances should be non-null.
  ASSERT_TRUE(light_color_provider);
  ASSERT_TRUE(dark_color_provider);
  ASSERT_TRUE(forced_colors_color_provider);
}

}  // namespace blink
