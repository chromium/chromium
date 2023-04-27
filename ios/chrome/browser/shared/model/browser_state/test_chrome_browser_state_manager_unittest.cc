// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using TestChromeBrowserStateManagerTest = PlatformTest;

// Tests that the list of loaded browser states is empty after invoking the
// constructor that accepts a user data directory path.
TEST_F(TestChromeBrowserStateManagerTest, ConstructWithUserDataDirPath) {
  web::WebTaskEnvironment task_environment;
  TestChromeBrowserStateManager browser_state_manager((base::FilePath()));
  EXPECT_EQ(0U, browser_state_manager.GetLoadedBrowserStates().size());
}

// Tests that the list of loaded browser states has one element after invoking
// the constructor that accepts a browser state.
TEST_F(TestChromeBrowserStateManagerTest, ConstructWithBrowserState) {
  web::WebTaskEnvironment task_environment;
  TestChromeBrowserStateManager browser_state_manager(
      TestChromeBrowserState::Builder().Build());
  EXPECT_EQ(1U, browser_state_manager.GetLoadedBrowserStates().size());
}
