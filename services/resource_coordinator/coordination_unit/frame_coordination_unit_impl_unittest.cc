// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class FrameCoordinationUnitImplTest : public CoordinationUnitTestHarness {
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

using FrameCoordinationUnitImplDeathTest = FrameCoordinationUnitImplTest;

}  // namespace

TEST_F(FrameCoordinationUnitImplTest, AddChildFrameBasic) {
  auto frame1_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame2_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame3_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  frame1_cu->AddChildFrame(frame2_cu->id());
  frame1_cu->AddChildFrame(frame3_cu->id());
  EXPECT_EQ(nullptr, frame1_cu->GetParentFrameCoordinationUnit());
  EXPECT_EQ(2u, frame1_cu->child_frame_coordination_units_for_testing().size());
  EXPECT_EQ(frame1_cu.get(), frame2_cu->GetParentFrameCoordinationUnit());
  EXPECT_EQ(frame1_cu.get(), frame3_cu->GetParentFrameCoordinationUnit());
}

TEST_F(FrameCoordinationUnitImplDeathTest, AddChildFrameOnCyclicReference) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  auto frame1_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame2_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame3_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  frame1_cu->AddChildFrame(frame2_cu->id());
  frame2_cu->AddChildFrame(frame3_cu->id());
// |frame3_cu| can't add |frame1_cu| because |frame1_cu| is an ancestor of
// |frame3_cu|, and this will hit a DCHECK because of cyclic reference.
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH_IF_SUPPORTED(frame3_cu->AddChildFrame(frame1_cu->id()), "");
#else
  frame3_cu->AddChildFrame(frame1_cu->id());
#endif  // !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)

  EXPECT_EQ(1u, frame1_cu->child_frame_coordination_units_for_testing().count(
                    frame2_cu.get()));
  EXPECT_EQ(1u, frame2_cu->child_frame_coordination_units_for_testing().count(
                    frame3_cu.get()));
  // |frame1_cu| was not added successfully because |frame1_cu| is one of the
  // ancestors of |frame3_cu|.
  EXPECT_EQ(0u, frame3_cu->child_frame_coordination_units_for_testing().count(
                    frame1_cu.get()));
}

TEST_F(FrameCoordinationUnitImplTest, RemoveChildFrame) {
  auto parent_frame_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto child_frame_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  // Parent-child relationships have not been established yet.
  EXPECT_EQ(
      0u, parent_frame_cu->child_frame_coordination_units_for_testing().size());
  EXPECT_TRUE(!parent_frame_cu->GetParentFrameCoordinationUnit());
  EXPECT_EQ(
      0u, child_frame_cu->child_frame_coordination_units_for_testing().size());
  EXPECT_TRUE(!child_frame_cu->GetParentFrameCoordinationUnit());

  parent_frame_cu->AddChildFrame(child_frame_cu->id());

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(
      1u, parent_frame_cu->child_frame_coordination_units_for_testing().size());
  EXPECT_TRUE(!parent_frame_cu->GetParentFrameCoordinationUnit());
  EXPECT_EQ(
      0u, child_frame_cu->child_frame_coordination_units_for_testing().size());
  EXPECT_EQ(parent_frame_cu.get(),
            child_frame_cu->GetParentFrameCoordinationUnit());

  parent_frame_cu->RemoveChildFrame(child_frame_cu->id());

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(
      0u, parent_frame_cu->child_frame_coordination_units_for_testing().size());
  EXPECT_TRUE(!parent_frame_cu->GetParentFrameCoordinationUnit());
  EXPECT_EQ(
      0u, child_frame_cu->child_frame_coordination_units_for_testing().size());
  EXPECT_TRUE(!child_frame_cu->GetParentFrameCoordinationUnit());
}

TEST_F(FrameCoordinationUnitImplTest, LastAudibleTime) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  EXPECT_EQ(base::TimeTicks(), cu_graph.frame->last_audible_time());
  cu_graph.frame->SetAudibility(true);
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  cu_graph.frame->SetAudibility(false);
  EXPECT_EQ(ResourceCoordinatorClock::NowTicks(),
            cu_graph.frame->last_audible_time());
}

int64_t GetLifecycleState(PageCoordinationUnitImpl* cu) {
  int64_t value;
  if (cu->GetProperty(mojom::PropertyType::kLifecycleState, &value))
    return value;
  // Initial state is running.
  return static_cast<int64_t>(mojom::LifecycleState::kRunning);
}

#define EXPECT_FROZEN(cu)                                         \
  EXPECT_EQ(static_cast<int64_t>(mojom::LifecycleState::kFrozen), \
            GetLifecycleState(cu.get()))
#define EXPECT_RUNNING(cu)                                         \
  EXPECT_EQ(static_cast<int64_t>(mojom::LifecycleState::kRunning), \
            GetLifecycleState(cu.get()))

TEST_F(FrameCoordinationUnitImplTest, LifecycleStatesTransitions) {
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph(
      coordination_unit_graph());
  // Verifying the model.
  ASSERT_TRUE(cu_graph.frame->IsMainFrame());
  ASSERT_TRUE(cu_graph.other_frame->IsMainFrame());
  ASSERT_FALSE(cu_graph.child_frame->IsMainFrame());
  ASSERT_EQ(cu_graph.child_frame->GetParentFrameCoordinationUnit(),
            cu_graph.other_frame.get());
  ASSERT_EQ(cu_graph.frame->GetPageCoordinationUnit(), cu_graph.page.get());
  ASSERT_EQ(cu_graph.other_frame->GetPageCoordinationUnit(),
            cu_graph.other_page.get());

  // Freezing a child frame should not affect the page state.
  cu_graph.child_frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  EXPECT_RUNNING(cu_graph.page);
  EXPECT_RUNNING(cu_graph.other_page);

  // Freezing the only frame in a page should freeze that page.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  EXPECT_FROZEN(cu_graph.page);
  EXPECT_RUNNING(cu_graph.other_page);

  // Unfreeze the child frame in the other page.
  cu_graph.child_frame->SetLifecycleState(mojom::LifecycleState::kRunning);
  EXPECT_FROZEN(cu_graph.page);
  EXPECT_RUNNING(cu_graph.other_page);

  // Freezing the main frame in the other page should not alter that pages
  // state, as there is still a child frame that is running.
  cu_graph.other_frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  EXPECT_FROZEN(cu_graph.page);
  EXPECT_RUNNING(cu_graph.other_page);

  // Refreezing the child frame should freeze the page.
  cu_graph.child_frame->SetLifecycleState(mojom::LifecycleState::kFrozen);
  EXPECT_FROZEN(cu_graph.page);
  EXPECT_FROZEN(cu_graph.other_page);

  // Unfreezing a main frame should unfreeze the associated page.
  cu_graph.frame->SetLifecycleState(mojom::LifecycleState::kRunning);
  EXPECT_RUNNING(cu_graph.page);
  EXPECT_FROZEN(cu_graph.other_page);

  // Unfreezing the child frame should unfreeze the associated page.
  cu_graph.child_frame->SetLifecycleState(mojom::LifecycleState::kRunning);
  EXPECT_RUNNING(cu_graph.page);
  EXPECT_RUNNING(cu_graph.other_page);

  // Unfreezing the main frame shouldn't change anything.
  cu_graph.other_frame->SetLifecycleState(mojom::LifecycleState::kRunning);
  EXPECT_RUNNING(cu_graph.page);
  EXPECT_RUNNING(cu_graph.other_page);
}

}  // namespace resource_coordinator
