// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/list_family_members_service_factory.h"

#import "components/supervised_user/core/browser/list_family_members_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing ListFamilyMembersServiceFactory class.
class ListFamilyMembersServiceFactoryTest : public PlatformTest {
 protected:
  ListFamilyMembersServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that ListFamilyMembersServiceFactory creates
// ListFamilyMembersService.
TEST_F(ListFamilyMembersServiceFactoryTest, CreateService) {
  supervised_user::ListFamilyMembersService* service =
      ListFamilyMembersServiceFactory::GetForBrowserState(browser_state_.get());
  ASSERT_TRUE(service);
}

// Tests that ListFamilyMembersServiceFactory retuns null
// with an off-the-record ChromeBrowserState.
TEST_F(ListFamilyMembersServiceFactoryTest,
       ReturnsNullOnOffTheRecordBrowserState) {
  ChromeBrowserState* otr_browser_state =
      browser_state_->CreateOffTheRecordBrowserStateWithTestingFactories({});
  CHECK(otr_browser_state);
  supervised_user::ListFamilyMembersService* service =
      ListFamilyMembersServiceFactory::GetForBrowserState(otr_browser_state);
  ASSERT_FALSE(service);
}
