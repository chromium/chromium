// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"

#include "base/test/test_mock_time_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/strict_yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/unyielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class DisplayLockBudgetTest : public RenderingTest,
                              private ScopedDisplayLockingForTest {
 public:
  DisplayLockBudgetTest() : ScopedDisplayLockingForTest(true) {}
  void SetUp() override {
    RenderingTest::SetUp();
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

  base::TimeDelta GetBudget(const YieldingDisplayLockBudget& budget,
                            const LifecycleData& lifecycle_data) const {
    return budget.GetCurrentBudget(lifecycle_data);
  }

  void ResetDeadlineForTesting(YieldingDisplayLockBudget& budget,
                               const LifecycleData& lifecycle_data) {
    budget.deadline_ =
        base::TimeTicks::Now() + budget.GetCurrentBudget(lifecycle_data);
  }

  void ResetBudget(std::unique_ptr<DisplayLockBudget> budget,
                   DisplayLockContext* context) {
    ASSERT_TRUE(context->update_budget_);
    context->update_budget_ = std::move(budget);
  }

  void LockElement(Element& element, bool update_lifecycle = true) {
    element.setAttribute(html_names::kRendersubtreeAttr, "invisible");
    if (update_lifecycle)
      UpdateAllLifecyclePhasesForTest();
  }

  void CommitElement(Element& element, bool update_lifecycle = true) {
    element.setAttribute(html_names::kRendersubtreeAttr, "");
    if (update_lifecycle)
      UpdateAllLifecyclePhasesForTest();
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;

 private:
  base::Optional<RuntimeEnabledFeatures::Backup> features_backup_;
};

TEST_F(DisplayLockBudgetTest, UnyieldingBudget) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetHtmlInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  LockElement(*element, false);

  ASSERT_TRUE(element->GetDisplayLockContext());
  UnyieldingDisplayLockBudget budget(element->GetDisplayLockContext());

  // When acquiring, we need to update the layout with the locked size, so we
  // need an update.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  LifecycleData lifecycle_data;

  // Check everything twice since it shouldn't matter how many times we ask the
  // unyielding budget, the results should always be the same.
  for (int i = 0; i < 2; ++i) {
    // Note that although we only dirtied layout, all phases "should" complete,
    // since the budget should never be responsible for blocking phases for any
    // reason other than we're out of budget.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
  }
}

TEST_F(DisplayLockBudgetTest, StrictYieldingBudget) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  LockElement(*element, false);

  ASSERT_TRUE(element->GetDisplayLockContext());
  StrictYieldingDisplayLockBudget budget(element->GetDisplayLockContext());

  // When acquiring, we need to update the layout with the locked size, so we
  // need an update.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  LifecycleData lifecycle_data;

  {
    // Initially all of the phase checks should return true, since we don't know
    // which phase the system wants to process next.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // Not doing anything should ensure that we schedule another animation by
    // returning true here.
    EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  }
  {
    lifecycle_data.count++;
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // Once we perform a phase, its check should remain true, but the rest
    // will be false for this cycle.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                           lifecycle_data));
    EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                           lifecycle_data));
    if (true)
      return;

    // We would need at least one more run to finish everything.
    EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  }
  {
    lifecycle_data.count++;
    // Run the previous block again, now everything will always return true
    // since the phase we complete here (style) has already been completed
    // before, and we are open to complete a new phase.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // Since we already befored style before, no new phase has been processed
    // and all phases are allowed to finish.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // We would need at least one more run to finish everything.
    EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  }
  {
    lifecycle_data.count++;
    // On the next run, the checks for phases completed before should always
    // return true, and as before since we haven't completed a new phase, the
    // remainder of the phases should return true for now.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // This check is the same as in the previous block, but is here to verify
    // that going through NeedsLifecycleUpdates() and then
    // WillStartLifecycleUpdate() again doesn't change the fact that we should
    // still perform all of the phases at this point.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // Let's say layout was clean and we jumped and did prepaint instead, now
    // every phase before and including prepaint should be true, the rest are
    // locked from completing.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // Note that since we processed everything, we no longer need lifecycle
    // updates.
    EXPECT_FALSE(budget.NeedsLifecycleUpdates());
  }
  {
    // Do one more run to ensure everything is still returning true.
    lifecycle_data.count++;
    // On the last run, we'll complete all phases. Since there is only one
    // remaining phase we haven't done, all of the checks should always return
    // true (it's either an old phase or a first uncompleted phase).
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));

    // Since we completed everything, we should now be returning false here (no
    // more updates needed).
    EXPECT_FALSE(budget.NeedsLifecycleUpdates());
  }
}

TEST_F(DisplayLockBudgetTest,
       StrictYieldingBudgetOnlyNeedsUpdatesForDirtyPhases) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetHtmlInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  LockElement(*element, false);

  ASSERT_TRUE(element->GetDisplayLockContext());
  StrictYieldingDisplayLockBudget budget(element->GetDisplayLockContext());

  // When acquiring, we need to update the layout with the locked size, so we
  // need an update.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  LifecycleData lifecycle_data;
  budget.OnLifecycleChange(lifecycle_data);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                        lifecycle_data));
  budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  lifecycle_data.count++;
  budget.OnLifecycleChange(lifecycle_data);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                        lifecycle_data));
  budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);

  // Note that since the layout was indicated as done (from the budget
  // perspective), it will no longer need updates even though the true layout is
  // dirty. This is because it will no longer block layout from synchronously
  // completing whenever necessary.
  EXPECT_FALSE(budget.NeedsLifecycleUpdates());
}

TEST_F(DisplayLockBudgetTest, YieldingBudget) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  LockElement(*element, false);

  ASSERT_TRUE(element->GetDisplayLockContext());
  YieldingDisplayLockBudget budget(element->GetDisplayLockContext());
  budget.SetTickClockForTesting(test_task_runner_->GetMockTickClock());

  // When acquiring, we need to update the layout with the locked size, so we
  // need an update.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  LifecycleData lifecycle_data;
  budget.OnLifecycleChange(lifecycle_data);

  // Initially all of the phase checks should return true, since we don't know
  // which phase the system wants to process next.
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                        lifecycle_data));

  // Not doing anything should ensure that we schedule another animation by
  // returning true here.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  // Advancing the clock a bit will make us still want to the phases.
  test_task_runner_->FastForwardBy(GetBudget(budget, lifecycle_data) / 2);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                        lifecycle_data));

  // However, once we're out of budget, we will only do the next phase.
  test_task_runner_->FastForwardBy(GetBudget(budget, lifecycle_data));

  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                         lifecycle_data));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                         lifecycle_data));

  // Starting a new lifecycle will reset the budget.
  lifecycle_data.count++;
  lifecycle_data.start_time = test_task_runner_->NowTicks();
  budget.OnLifecycleChange(lifecycle_data);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                        lifecycle_data));

  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                        lifecycle_data));

  // Performing a phase still keeps the rest of the phases available for work
  // since we haven't advanced the clock.
  budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                        lifecycle_data));

  // Now that we're out of budget, phases performed previously should remain
  // true.
  test_task_runner_->FastForwardBy(GetBudget(budget, lifecycle_data) * 2);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                         lifecycle_data));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                         lifecycle_data));

  // Sanity check here: the element still needs layout.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  // Resetting the budget, and advancing again should yield the same results as
  // before, except that we will process at least one more phase.
  lifecycle_data.count++;
  lifecycle_data.start_time = test_task_runner_->NowTicks();
  budget.OnLifecycleChange(lifecycle_data);

  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                        lifecycle_data));
  test_task_runner_->FastForwardBy(GetBudget(budget, lifecycle_data) * 2);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                        lifecycle_data));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                        lifecycle_data));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                         lifecycle_data));

  // Eventually the budget becomes essentially infinite.
  lifecycle_data.count += 60;
  budget.OnLifecycleChange(lifecycle_data);

  EXPECT_GT(GetBudget(budget, lifecycle_data),
            base::TimeDelta::FromMilliseconds(1e6));
  for (int i = 0; i < 60; ++i) {
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                          lifecycle_data));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                          lifecycle_data));
    test_task_runner_->FastForwardBy(base::TimeDelta::FromMillisecondsD(10000));
  }
}

TEST_F(DisplayLockBudgetTest, YieldingBudgetMarksNextPhase) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetHtmlInnerHTML(R"HTML(
    <style>
      #container {
        contain: style layout;
      }
    </style>
    <div id="parent"><div id="container"><div id="child"></div></div></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  LockElement(*element);

  ASSERT_TRUE(element->GetDisplayLockContext());
  ASSERT_TRUE(element->GetDisplayLockContext()->IsLocked());

  auto budget_owned = base::WrapUnique(
      new YieldingDisplayLockBudget(element->GetDisplayLockContext()));
  auto* budget = budget_owned.get();
  budget->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->GetDisplayLockContext()->UpdateRendering(script_state);
    ResetBudget(std::move(budget_owned), element->GetDisplayLockContext());
  }

  // When acquiring, we need to update the layout with the locked size, so we
  // need an update.
  EXPECT_TRUE(budget->NeedsLifecycleUpdates());

  // Dirtying the element will cause us to do updates.
  GetDocument().getElementById("child")->SetInnerHTMLFromString("a");

  auto* parent = GetDocument().getElementById("parent");
  EXPECT_TRUE(budget->NeedsLifecycleUpdates());

  LifecycleData lifecycle_data;
  budget->OnLifecycleChange(lifecycle_data);

  GetDocument().View()->SetInLifecycleUpdateForTest(true);
  GetDocument().View()->SetLifecycleDataForTesting(lifecycle_data);

  // Initially all of the phase checks should return true, since we don't know
  // which phase the system wants to process next.
  EXPECT_TRUE(
      budget->ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                 GetDocument().View()->CurrentLifecycleData()));
  EXPECT_TRUE(
      budget->ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                 GetDocument().View()->CurrentLifecycleData()));
  EXPECT_TRUE(
      budget->ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                 GetDocument().View()->CurrentLifecycleData()));

  EXPECT_TRUE(parent->NeedsStyleRecalc() || parent->ChildNeedsStyleRecalc());
  EXPECT_TRUE(element->NeedsStyleRecalc() || element->ChildNeedsStyleRecalc());

  test_task_runner_->FastForwardBy(
      GetBudget(*budget, GetDocument().View()->CurrentLifecycleData()) * 2);
  EXPECT_TRUE(
      budget->ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                 GetDocument().View()->CurrentLifecycleData()));
  EXPECT_FALSE(
      budget->ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                 GetDocument().View()->CurrentLifecycleData()));

  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(
      budget->ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                 GetDocument().View()->CurrentLifecycleData()));

  EXPECT_FALSE(parent->NeedsStyleRecalc() || parent->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc() || element->ChildNeedsStyleRecalc());

  EXPECT_TRUE(parent->GetLayoutObject()->NeedsLayout());
  EXPECT_TRUE(element->GetLayoutObject()->NeedsLayout());

  ResetDeadlineForTesting(*budget,
                          GetDocument().View()->CurrentLifecycleData());
  EXPECT_TRUE(
      budget->ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                 GetDocument().View()->CurrentLifecycleData()));

  GetDocument().View()->SetInLifecycleUpdateForTest(false);
}

TEST_F(DisplayLockBudgetTest, UpdateHappensInLifecycleOnly) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetHtmlInnerHTML(R"HTML(
    <style>
      #container {
        contain: style layout;
      }
    </style>
    <div id="parent"><div id="container"><div id="child"></div></div></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  LockElement(*element);

  ASSERT_TRUE(element->GetDisplayLockContext());
  ASSERT_TRUE(element->GetDisplayLockContext()->IsLocked());

  auto budget_owned = base::WrapUnique(
      new UnyieldingDisplayLockBudget(element->GetDisplayLockContext()));
  auto* budget = budget_owned.get();
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->GetDisplayLockContext()->UpdateRendering(script_state);
    ResetBudget(std::move(budget_owned), element->GetDisplayLockContext());
  }

  // When acquiring, we need to update the layout with the locked size, so we
  // need an update.
  EXPECT_TRUE(budget->NeedsLifecycleUpdates());

  LifecycleData lifecycle_data;
  budget->OnLifecycleChange(lifecycle_data);

  auto* context = element->GetDisplayLockContext();
  EXPECT_TRUE(budget->ShouldPerformPhase(DisplayLockBudget::Phase::kStyle,
                                         lifecycle_data));
  EXPECT_TRUE(budget->ShouldPerformPhase(DisplayLockBudget::Phase::kLayout,
                                         lifecycle_data));
  EXPECT_TRUE(budget->ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint,
                                         lifecycle_data));

  // Since we're not in a lifecycle, the budget itself should not want to do any
  // phases, even though the budget allows it.
  EXPECT_FALSE(context->ShouldStyle(DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(context->ShouldLayout(DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(context->ShouldPrePaint(DisplayLockLifecycleTarget::kChildren));

  GetDocument().GetFrame()->View()->SetInLifecycleUpdateForTest(true);

  EXPECT_TRUE(context->ShouldStyle(DisplayLockLifecycleTarget::kChildren));
  EXPECT_TRUE(context->ShouldLayout(DisplayLockLifecycleTarget::kChildren));
  EXPECT_TRUE(context->ShouldPrePaint(DisplayLockLifecycleTarget::kChildren));

  GetDocument().GetFrame()->View()->SetInLifecycleUpdateForTest(false);

  EXPECT_FALSE(context->ShouldStyle(DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(context->ShouldLayout(DisplayLockLifecycleTarget::kChildren));
  EXPECT_FALSE(context->ShouldPrePaint(DisplayLockLifecycleTarget::kChildren));

  // Ensure to flush any tasks scheduled by context calls.
  test::RunPendingTasks();
}
}  // namespace blink
