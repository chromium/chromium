// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class PageCoordinationUnitImplTest : public CoordinationUnitTestHarness {
 public:
  void SetUp() override {
    ResourceCoordinatorClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override {
    ResourceCoordinatorClock::ResetClockForTesting();
  }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

}  // namespace

TEST_F(PageCoordinationUnitImplTest, AddFrameBasic) {
  auto page_cu = CreateCoordinationUnit<PageCoordinationUnitImpl>();
  auto frame1_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame2_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame3_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  page_cu->AddFrame(frame1_cu->id());
  page_cu->AddFrame(frame2_cu->id());
  page_cu->AddFrame(frame3_cu->id());
  EXPECT_EQ(3u, page_cu->GetFrameCoordinationUnits().size());
}

TEST_F(PageCoordinationUnitImplTest, AddReduplicativeFrame) {
  auto page_cu = CreateCoordinationUnit<PageCoordinationUnitImpl>();
  auto frame1_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame2_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  page_cu->AddFrame(frame1_cu->id());
  page_cu->AddFrame(frame2_cu->id());
  page_cu->AddFrame(frame1_cu->id());
  EXPECT_EQ(2u, page_cu->GetFrameCoordinationUnits().size());
}

TEST_F(PageCoordinationUnitImplTest, RemoveFrame) {
  auto page_cu = CreateCoordinationUnit<PageCoordinationUnitImpl>();
  auto frame_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  // Parent-child relationships have not been established yet.
  EXPECT_EQ(0u, page_cu->GetFrameCoordinationUnits().size());
  EXPECT_FALSE(frame_cu->GetPageCoordinationUnit());

  page_cu->AddFrame(frame_cu->id());

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(1u, page_cu->GetFrameCoordinationUnits().size());
  EXPECT_EQ(1u, page_cu->GetFrameCoordinationUnits().count(frame_cu.get()));
  EXPECT_EQ(page_cu.get(), frame_cu->GetPageCoordinationUnit());

  page_cu->RemoveFrame(frame_cu->id());

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, page_cu->GetFrameCoordinationUnits().size());
  EXPECT_FALSE(frame_cu->GetPageCoordinationUnit());
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  cu_graph.process->SetCPUUsage(40);
  EXPECT_EQ(40, cu_graph.page->GetCPUUsage());
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  cu_graph.process->SetCPUUsage(40);
  EXPECT_EQ(20, cu_graph.page->GetCPUUsage());
  EXPECT_EQ(20, cu_graph.other_page->GetCPUUsage());
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  cu_graph.process->SetCPUUsage(40);
  cu_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(70, cu_graph.page->GetCPUUsage());
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  cu_graph.process->SetCPUUsage(40);
  cu_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(20, cu_graph.page->GetCPUUsage());
  EXPECT_EQ(50, cu_graph.other_page->GetCPUUsage());
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageEQTForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));

  int64_t eqt;
  EXPECT_TRUE(cu_graph.page->GetExpectedTaskQueueingDuration(&eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageEQTForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));

  int64_t eqt;
  EXPECT_TRUE(cu_graph.page->GetExpectedTaskQueueingDuration(&eqt));
  EXPECT_EQ(1, eqt);
  eqt = 0;
  EXPECT_TRUE(cu_graph.other_page->GetExpectedTaskQueueingDuration(&eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageCoordinationUnitImplTest, TimeSinceLastVisibilityChange) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  cu_graph.page->SetVisibility(true);
  EXPECT_TRUE(cu_graph.page->IsVisible());
  AdvanceClock(base::TimeDelta::FromSeconds(42));
  EXPECT_EQ(base::TimeDelta::FromSeconds(42),
            cu_graph.page->TimeSinceLastVisibilityChange());

  cu_graph.page->SetVisibility(false);
  AdvanceClock(base::TimeDelta::FromSeconds(23));
  EXPECT_EQ(base::TimeDelta::FromSeconds(23),
            cu_graph.page->TimeSinceLastVisibilityChange());
  EXPECT_FALSE(cu_graph.page->IsVisible());
}

TEST_F(PageCoordinationUnitImplTest, TimeSinceLastNavigation) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(cu_graph.page->TimeSinceLastNavigation().is_zero());

  // 1st navigation.
  cu_graph.page->OnMainFrameNavigationCommitted(
      ResourceCoordinatorClock::NowTicks(), 10u, "http://www.example.org");
  EXPECT_EQ("http://www.example.org", cu_graph.page->main_frame_url());
  EXPECT_EQ(10u, cu_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta::FromSeconds(11),
            cu_graph.page->TimeSinceLastNavigation());

  // 2nd navigation.
  cu_graph.page->OnMainFrameNavigationCommitted(
      ResourceCoordinatorClock::NowTicks(), 20u,
      "http://www.example.org/bobcat");
  EXPECT_EQ("http://www.example.org/bobcat", cu_graph.page->main_frame_url());
  EXPECT_EQ(20u, cu_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(17));
  EXPECT_EQ(base::TimeDelta::FromSeconds(17),
            cu_graph.page->TimeSinceLastNavigation());
}

TEST_F(PageCoordinationUnitImplTest, IsLoading) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  auto* page_cu = cu_graph.page.get();

  // First attempt should fail, as the property is unset.
  int64_t loading = 0;
  EXPECT_FALSE(page_cu->GetProperty(mojom::PropertyType::kIsLoading, &loading));

  // Set to false and the property should read false.
  page_cu->SetIsLoading(false);
  EXPECT_TRUE(page_cu->GetProperty(mojom::PropertyType::kIsLoading, &loading));
  EXPECT_EQ(0u, loading);

  // Set to true and the property should read true.
  page_cu->SetIsLoading(true);
  EXPECT_TRUE(page_cu->GetProperty(mojom::PropertyType::kIsLoading, &loading));
  EXPECT_EQ(1u, loading);

  // Set to false and the property should read false again.
  page_cu->SetIsLoading(false);
  EXPECT_TRUE(page_cu->GetProperty(mojom::PropertyType::kIsLoading, &loading));
  EXPECT_EQ(0u, loading);
}

TEST_F(PageCoordinationUnitImplTest, OnAllFramesInPageFrozen) {
  const int64_t kRunning =
      static_cast<int64_t>(mojom::LifecycleState::kRunning);
  const int64_t kFrozen = static_cast<int64_t>(mojom::LifecycleState::kFrozen);

  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  EXPECT_EQ(kRunning, cu_graph.page->GetPropertyOrDefault(
                          mojom::PropertyType::kLifecycleState, kRunning));

  // 1/2 frames in the page is frozen. Expect the page to still be running.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  EXPECT_EQ(kRunning, cu_graph.page->GetPropertyOrDefault(
                          mojom::PropertyType::kLifecycleState, kRunning));

  // 2/2 frames in the process are frozen. We expect the page to be frozen.
  cu_graph.child_frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  EXPECT_EQ(kFrozen, cu_graph.page->GetPropertyOrDefault(
                         mojom::PropertyType::kLifecycleState, kRunning));

  // Unfreeze a frame and expect the page to be running again.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kRunning);
  EXPECT_EQ(kRunning, cu_graph.page->GetPropertyOrDefault(
                          mojom::PropertyType::kLifecycleState, kRunning));

  // Refreeze that frame and expect the page to be frozen again.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  EXPECT_EQ(kFrozen, cu_graph.page->GetPropertyOrDefault(
                         mojom::PropertyType::kLifecycleState, kRunning));
}

}  // namespace resource_coordinator
