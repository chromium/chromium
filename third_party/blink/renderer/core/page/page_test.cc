// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page.h"

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"

namespace blink {

TEST(PageTest, CreateOrdinaryBrowsingContextGroup) {
  EmptyChromeClient client;
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();
  auto bcg_info = BrowsingContextGroupInfo::CreateUnique();

  Page* page =
      Page::CreateOrdinary(client, /*opener=*/nullptr, *scheduler, bcg_info);

  EXPECT_EQ(page->BrowsingContextGroupToken(),
            bcg_info.browsing_context_group_token);
  EXPECT_EQ(page->CoopRelatedGroupToken(), bcg_info.coop_related_group_token);
}

TEST(PageTest, CreateNonOrdinaryBrowsingContextGroup) {
  EmptyChromeClient client;
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();

  Page* page = Page::CreateNonOrdinary(client, *scheduler);

  EXPECT_FALSE(page->BrowsingContextGroupToken().is_empty());
  EXPECT_FALSE(page->CoopRelatedGroupToken().is_empty());

  EXPECT_NE(page->BrowsingContextGroupToken(), page->CoopRelatedGroupToken());
}

TEST(PageTest, BrowsingContextGroupUpdate) {
  EmptyChromeClient client;
  auto* scheduler = scheduler::CreateDummyAgentGroupScheduler();
  auto initial_bcg_info = BrowsingContextGroupInfo::CreateUnique();

  Page* page = Page::CreateOrdinary(client, /*opener=*/nullptr, *scheduler,
                                    initial_bcg_info);

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

}  // namespace blink
