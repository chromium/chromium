// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/apple/url_conversions.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
const char kUrl[] = "https://test.test/calendar.ics";
}  // namespace

class SafariDownloadTabHelperTest : public PlatformTest {
 protected:
  SafariDownloadTabHelperTest() {
    SafariDownloadTabHelper::CreateForWebState(&web_state_);
    web_state_.WasShown();
  }

  SafariDownloadTabHelper* tab_helper() {
    return SafariDownloadTabHelper::FromWebState(&web_state_);
  }

  base::test::TaskEnvironment task_environment_;
  web::FakeWebState web_state_;
};

// Tests that a calendar alert deferred while the WebState was hidden is
// dropped upon observing a cross-document navigation start.
TEST_F(SafariDownloadTabHelperTest, DeferredCalendarDroppedOnNavigationStart) {
  web_state_.WasHidden();

  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "text/calendar");

  id mock_delegate =
      OCMProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
  tab_helper()->set_delegate(mock_delegate);
  [[mock_delegate reject] presentCalendarAlertFromURL:OCMOCK_ANY];

  tab_helper()->DownloadCalendar(std::move(task));
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  web_state_.OnNavigationStarted(&context);

  id mock_delegate_visible =
      OCMProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
  tab_helper()->set_delegate(mock_delegate_visible);
  [[mock_delegate_visible reject] presentCalendarAlertFromURL:OCMOCK_ANY];

  web_state_.WasShown();
  EXPECT_OCMOCK_VERIFY(mock_delegate_visible);
}

// Tests that a calendar alert deferred during provisional navigation is
// dropped upon observing a committed cross-document navigation finish.
TEST_F(SafariDownloadTabHelperTest, DeferredCalendarDroppedOnNavigationFinish) {
  web_state_.WasHidden();

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);

  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "text/calendar");

  id mock_delegate =
      OCMProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
  tab_helper()->set_delegate(mock_delegate);
  [[mock_delegate reject] presentCalendarAlertFromURL:OCMOCK_ANY];

  tab_helper()->DownloadCalendar(std::move(task));
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  web_state_.OnNavigationFinished(&context);

  id mock_delegate_visible =
      OCMProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
  tab_helper()->set_delegate(mock_delegate_visible);
  [[mock_delegate_visible reject] presentCalendarAlertFromURL:OCMOCK_ANY];

  web_state_.WasShown();
  EXPECT_OCMOCK_VERIFY(mock_delegate_visible);
}

// Tests that dismissDownloadAlert is called upon observing a cross-document
// navigation start.
TEST_F(SafariDownloadTabHelperTest, DismissDownloadAlertOnNavigationStart) {
  id mock_delegate =
      OCMProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
  tab_helper()->set_delegate(mock_delegate);
  OCMExpect([mock_delegate dismissDownloadAlert]);

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  web_state_.OnNavigationStarted(&context);

  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that dismissDownloadAlert is called upon observing a committed
// cross-document navigation finish.
TEST_F(SafariDownloadTabHelperTest, DismissDownloadAlertOnNavigationFinish) {
  id mock_delegate =
      OCMProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
  tab_helper()->set_delegate(mock_delegate);
  OCMExpect([mock_delegate dismissDownloadAlert]);

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(mock_delegate);
}
