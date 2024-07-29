// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using TestChromeBrowserStateManagerTest = PlatformTest;

// Tests that the list of loaded browser states is empty after construction.
TEST_F(TestChromeBrowserStateManagerTest, Constructor) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestChromeBrowserStateManager browser_state_manager;
  EXPECT_EQ(0U, browser_state_manager.GetLoadedBrowserStates().size());
}

// Tests that the list of loaded browser states has one element after calling
// AddBrowserStateWithBuilder(...).
TEST_F(TestChromeBrowserStateManagerTest, AddBrowserStateWithBuilder) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestChromeBrowserStateManager browser_state_manager;
  browser_state_manager.AddBrowserStateWithBuilder(
      TestChromeBrowserState::Builder());
  EXPECT_EQ(1U, browser_state_manager.GetLoadedBrowserStates().size());
}
