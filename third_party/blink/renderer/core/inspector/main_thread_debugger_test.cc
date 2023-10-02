// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
class MainThreadDebuggerTest : public PageTestBase {
};

TEST_F(MainThreadDebuggerTest, HitBreakPointDuringLifecycle) {
  Document& document = GetDocument();
  std::unique_ptr<DocumentLifecycle::PostponeTransitionScope>
      postponed_transition_scope =
          std::make_unique<DocumentLifecycle::PostponeTransitionScope>(
              document.Lifecycle());
  EXPECT_TRUE(document.Lifecycle().LifecyclePostponed());

  // The following steps would cause either style update or layout, it should
  // never crash.
  document.View()->ViewportSizeChanged();
  document.View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  document.UpdateStyleAndLayoutTree();

  postponed_transition_scope.reset();
  EXPECT_FALSE(document.Lifecycle().LifecyclePostponed());
}

class MainThreadDebuggerMultipleMainFramesTest
    : public MainThreadDebuggerTest,
      public testing::WithParamInterface<bool> {
 public:
  MainThreadDebuggerMultipleMainFramesTest() {
    if (IsDebuggerAllowed()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAllowDevToolsMainThreadDebuggerForMultipleMainFrames);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAllowDevToolsMainThreadDebuggerForMultipleMainFrames);
    }
  }
  ~MainThreadDebuggerMultipleMainFramesTest() override = default;

  void SetUp() override {
    second_dummy_page_holder_ = std::make_unique<DummyPageHolder>();
    MainThreadDebuggerTest::SetUp();
  }

  Page& GetSecondPage() { return second_dummy_page_holder_->GetPage(); }

  bool IsDebuggerAllowed() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<DummyPageHolder> second_dummy_page_holder_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         MainThreadDebuggerMultipleMainFramesTest,
                         testing::Bool());

TEST_P(MainThreadDebuggerMultipleMainFramesTest, Allow) {
  Page::InsertOrdinaryPageForTesting(&GetPage());
  Page::InsertOrdinaryPageForTesting(&GetSecondPage());
  GetFrame().GetSettings()->SetScriptEnabled(true);
  auto* debugger =
      MainThreadDebugger::Instance(GetDocument().GetAgent().isolate());
  int context_group_id = debugger->ContextGroupId(&GetFrame());

  if (IsDebuggerAllowed()) {
    ASSERT_TRUE(debugger->canExecuteScripts(context_group_id));
  } else {
    ASSERT_FALSE(debugger->canExecuteScripts(context_group_id));
  }
}

}  // namespace blink
