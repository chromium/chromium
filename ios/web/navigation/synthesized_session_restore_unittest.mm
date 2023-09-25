// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/synthesized_session_restore.h"

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace web {

namespace {

// Creates a vector with given number of navigation items. All items will
// have distinct titles, URLs, and referrers
void CreateTestNavigationItems(
    size_t count,
    std::vector<std::unique_ptr<NavigationItem>>& items) {
  for (size_t i = 0; i < count; i++) {
    auto item = std::make_unique<NavigationItemImpl>();
    item->SetURL(GURL(base::StringPrintf("http://www.%zu.com", i)));
    item->SetTitle(base::ASCIIToUTF16(base::StringPrintf("Test%zu", i)));
    // Set every other referrer.
    if (i % 2) {
      item->SetReferrer(web::Referrer(
          GURL(base::StringPrintf("http://www.referrer%zu.com", i)),
          static_cast<web::ReferrerPolicy>(0)));
    }
    items.push_back(std::move(item));
  }
}

}  // namespace

class SynthesizedSessionRestoreTest : public web::WebTest {
 protected:
  SynthesizedSessionRestoreTest() {}

  void SetUp() override {
    web::WebTest::SetUp();
    web::WebState::CreateParams params(GetBrowserState());
    web_state_ = std::make_unique<web::WebStateImpl>(params);
  }

  std::unique_ptr<WebStateImpl> web_state_;
};

// Test that the synthetic session data blob can be successfully loaded
// by WebStateImpl and correctly restores the session.
TEST_F(SynthesizedSessionRestoreTest, TestRestore) {
  std::vector<std::unique_ptr<NavigationItem>> items;
  CreateTestNavigationItems(100, items);

  NSData* synthesized_data = SynthesizedSessionRestore(
      /*last_committed_item_index=*/0, items, /*off_the_record=*/false);
  EXPECT_GT(synthesized_data.length, 0u);

  EXPECT_TRUE(web_state_->SetSessionStateData(synthesized_data));
  EXPECT_EQ(web_state_->GetNavigationItemCount(), 100);
}

}  // namespace web
