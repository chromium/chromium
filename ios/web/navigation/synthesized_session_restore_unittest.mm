// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/synthesized_session_restore.h"

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  SynthesizedSessionRestoreTest() {
    std::vector<base::test::FeatureRef> enabled;
    enabled.push_back(features::kSynthesizedRestoreSession);

    std::vector<base::test::FeatureRef> disabled;
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetUp() override {
    web::WebTest::SetUp();
    web::WebState::CreateParams params(GetBrowserState());
    web_state_ = std::make_unique<web::WebStateImpl>(params);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<WebStateImpl> web_state_;
  SynthesizedSessionRestore synthesized_restore_helper_;
};

TEST_F(SynthesizedSessionRestoreTest, TestLessThaniOS15) {
  if (base::ios::IsRunningOnIOS15OrLater())
    return;

  std::vector<std::unique_ptr<NavigationItem>> items;
  CreateTestNavigationItems(3, items);
  synthesized_restore_helper_.Init(0, items, false);
  EXPECT_FALSE(synthesized_restore_helper_.Restore(web_state_.get()));
}

TEST_F(SynthesizedSessionRestoreTest, TestRestore) {
  if (!base::ios::IsRunningOnIOS15OrLater())
    return;
  std::vector<std::unique_ptr<NavigationItem>> items;
  CreateTestNavigationItems(100, items);
  synthesized_restore_helper_.Init(0, items, false);

  EXPECT_TRUE(synthesized_restore_helper_.Restore(web_state_.get()));
  EXPECT_EQ(web_state_->GetNavigationItemCount(), 100);
}

}  // namespace web
