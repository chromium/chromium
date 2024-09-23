// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string_view>

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/ios/wait_util.h"
#import "base/test/mock_callback.h"
#import "base/test/task_environment.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/affiliations/core/browser/mock_affiliation_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/passwords/cwv_reuse_check_service_internal.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ios_web_view {

namespace {

using affiliations::Facet;
using affiliations::FacetURI;
using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using password_manager::PasswordForm;

PasswordForm GenerateSavedPassword(
    std::string_view signon_realm,
    std::u16string_view username,
    std::u16string_view password,
    std::u16string_view username_element = u"",
    PasswordForm::Store store = PasswordForm::Store::kProfileStore) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.username_element = std::u16string(username_element);
  form.in_store = store;
  return form;
}

constexpr int kDelay = 2000;

class CWVReuseCheckServiceTest : public PlatformTest {
 public:
  CWVReuseCheckServiceTest() { RunUntilIdle(); }

  void AdvanceClock(base::TimeDelta time) { task_env_.AdvanceClock(time); }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

}  // namespace

TEST_F(CWVReuseCheckServiceTest, CheckPasswords) {
  affiliations::MockAffiliationService mock_affiliation_service;
  affiliations::FakeAffiliationService affiliation_service_;
  CWVReuseCheckService* reuseCheckService = [[CWVReuseCheckService alloc]
      initWithAffiliationService:&affiliation_service_];

  PasswordForm form1 =
      GenerateSavedPassword("https://google.com/", u"user", u"pass");
  PasswordForm form2 =
      GenerateSavedPassword("https://yahoo.com/", u"user", u"pass");

  CWVPassword* password1 = [[CWVPassword alloc] initWithPasswordForm:form1];
  CWVPassword* password2 = [[CWVPassword alloc] initWithPasswordForm:form2];

  // Setup affiliated groups.
  std::vector<affiliations::GroupedFacets> grouped_facets(2);
  Facet facet1(FacetURI::FromPotentiallyInvalidSpec(form1.signon_realm));
  grouped_facets[0].facets.push_back(facet1);
  Facet facet2(FacetURI::FromPotentiallyInvalidSpec(form2.signon_realm));
  grouped_facets[1].facets.push_back(facet2);
  EXPECT_CALL(mock_affiliation_service, GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(grouped_facets));

  __block bool reuseCheckCompletionCalled = NO;
  id reuseCompletion = ^(NSSet<NSString*>* reusedPasswords) {
    ASSERT_EQ(1U, reusedPasswords.count);
    reuseCheckCompletionCalled = YES;
  };
  [reuseCheckService checkReusedPasswords:@[ password1, password2 ]
                        completionHandler:reuseCompletion];
  AdvanceClock(base::Milliseconds(kDelay));
  RunUntilIdle();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    RunUntilIdle();
    return reuseCheckCompletionCalled;
  }));
}

}  // namespace ios_web_view
