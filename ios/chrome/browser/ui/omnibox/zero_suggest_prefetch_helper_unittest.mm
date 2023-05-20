// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"

#import "base/test/task_environment.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/omnibox/browser/test_omnibox_edit_model_delegate.h"
#import "components/omnibox/browser/test_omnibox_view.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_client.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_legacy.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::FakeWebState;

namespace {

const char kTestURL[] = "http://chromium.org";
const char kTestSRPURL[] = "https://www.google.com/search?q=omnibox";

class MockOmniboxEditModel : public OmniboxEditModel {
 public:
  MockOmniboxEditModel(OmniboxController* omnibox_controller,
                       OmniboxView* view,
                       OmniboxEditModelDelegate* edit_model_delegate)
      : OmniboxEditModel(omnibox_controller, view, edit_model_delegate) {}

  ~MockOmniboxEditModel() override = default;
  MockOmniboxEditModel(const MockOmniboxEditModel&) = delete;
  MockOmniboxEditModel& operator=(const MockOmniboxEditModel&) = delete;

  // OmniboxEditModel:
  void StartPrefetch() override { call_count_++; }

  int call_count_ = 0;
};

}  // namespace

namespace {

class ZeroSuggestPrefetchHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    edit_model_delegate_ = std::make_unique<TestOmniboxEditModelDelegate>();
    view_ = std::make_unique<TestOmniboxView>(
        edit_model_delegate_.get(), std::make_unique<TestOmniboxClient>());
    model_ = std::make_unique<MockOmniboxEditModel>(
        view_->controller(), view_.get(), edit_model_delegate_.get());
  }

  void CreateHelper() {
    helper_ = [[ZeroSuggestPrefetchHelper alloc]
        initWithWebStateList:web_state_list_.get()
                   editModel:model_.get()];
  }
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;

  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;

  std::unique_ptr<TestOmniboxEditModelDelegate> edit_model_delegate_;
  std::unique_ptr<TestOmniboxView> view_;
  std::unique_ptr<MockOmniboxEditModel> model_;

  ZeroSuggestPrefetchHelper* helper_;
};

// Test that upon navigation, prefetch is called.
TEST_F(ZeroSuggestPrefetchHelperTest, TestReactToNavigation) {
  CreateHelper();
  web::FakeNavigationContext context;

  EXPECT_EQ(model_.get()->call_count_, 0);
  GURL not_ntp_url(kTestURL);
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(model_.get()->call_count_, 1);
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_EQ(model_.get()->call_count_, 2);
  model_.get()->call_count_ = 0;

  // Now navigate to NTP.
  EXPECT_EQ(model_.get()->call_count_, 0);

  GURL url(kChromeUINewTabURL);
  web_state_ptr->SetCurrentURL(url);
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_EQ(model_.get()->call_count_, 1);
  model_.get()->call_count_ = 0;

  // Now navigate to SRP.
  EXPECT_EQ(model_.get()->call_count_, 0);

  web_state_ptr->SetCurrentURL(GURL(kTestSRPURL));
  web_state_ptr->OnNavigationFinished(&context);

  EXPECT_EQ(model_.get()->call_count_, 1);
  model_.get()->call_count_ = 0;
}

// Test that switching between tabs starts prefetch.
TEST_F(ZeroSuggestPrefetchHelperTest, TestPrefetchOnTabSwitch) {
  CreateHelper();
  web::FakeNavigationContext context;

  EXPECT_EQ(model_.get()->call_count_, 0);
  GURL not_ntp_url(kTestURL);
  auto web_state = std::make_unique<web::FakeWebState>();
  FakeWebState* web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(model_.get()->call_count_, 1);
  web_state_ptr->OnNavigationFinished(&context);
  EXPECT_EQ(model_.get()->call_count_, 2);
  model_.get()->call_count_ = 0;

  // Second tab
  web_state = std::make_unique<web::FakeWebState>();
  web_state_ptr = web_state.get();
  web_state_ptr->SetCurrentURL(not_ntp_url);
  web_state_list_->InsertWebState(
      1, std::move(web_state), WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  web_state_list_->ActivateWebStateAt(1);
  EXPECT_EQ(model_.get()->call_count_, 1);
  model_.get()->call_count_ = 0;

  // Just switch
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(model_.get()->call_count_, 1);
  model_.get()->call_count_ = 0;
}

}  // namespace
