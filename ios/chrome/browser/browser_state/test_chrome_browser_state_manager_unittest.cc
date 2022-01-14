// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/web/chrome_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using TestChromeBrowserStateManagerTest = ChromeTest;

// Tests that the list of loaded browser states is empty after invoking the
// constructor that accepts a user data directory path.
TEST_F(TestChromeBrowserStateManagerTest, ConstructWithUserDataDirPath) {
  TestChromeBrowserStateManager browser_state_manager((base::FilePath()));
  EXPECT_EQ(0U, browser_state_manager.GetLoadedBrowserStates().size());
}

// Tests that the list of loaded browser states has one element after invoking
// the constructor that accepts a browser state.
TEST_F(TestChromeBrowserStateManagerTest, ConstructWithBrowserState) {
  TestChromeBrowserStateManager browser_state_manager(
      TestChromeBrowserState::Builder().Build());
  EXPECT_EQ(1U, browser_state_manager.GetLoadedBrowserStates().size());
}
