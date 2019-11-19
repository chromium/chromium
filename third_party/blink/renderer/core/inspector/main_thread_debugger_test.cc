// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
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
  document.View()->ViewportSizeChanged(true, true);
  document.View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  document.UpdateStyleAndLayout();
  document.UpdateStyleAndLayoutTree();

  postponed_transition_scope.reset();
  EXPECT_FALSE(document.Lifecycle().LifecyclePostponed());
}

}  // namespace blink
