// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/run_loop.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserAccessibilityStateImplTest : public ContentBrowserTest {
 public:
  BrowserAccessibilityStateImplTest() = default;
  ~BrowserAccessibilityStateImplTest() override = default;
};

// This test just gives some code coverage to the code that schedules
// background tasks in other threads in BrowserAccessibilityStateImpl.
// Note that we also have a bit of coverage in chrome/browser/accessibility.
IN_PROC_BROWSER_TEST_F(BrowserAccessibilityStateImplTest, TestBackgroundTasks) {
  base::RunLoop run_loop;
  content::BrowserAccessibilityStateImpl::GetInstance()
      ->CallInitBackgroundTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace content
