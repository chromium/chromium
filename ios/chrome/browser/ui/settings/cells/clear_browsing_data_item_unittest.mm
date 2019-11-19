// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_item.h"

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#include "ios/chrome/browser/browsing_data/cache_counter.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ClearDataItemTest : public PlatformTest {
 protected:
  void SetUp() override {
    // Setup identity services.
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
  }

  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::WebTaskEnvironment task_environment_;
};

// Test that if the counter is not set, then [item hasCounter] returns false.
TEST_F(ClearDataItemTest, ConfigureCellTestCounterNil) {
  std::unique_ptr<BrowsingDataCounterWrapper> counter;
  ClearBrowsingDataItem* item =
      [[ClearBrowsingDataItem alloc] initWithType:0 counter:std::move(counter)];
  EXPECT_FALSE([item hasCounter]);
}

}  // namespace
