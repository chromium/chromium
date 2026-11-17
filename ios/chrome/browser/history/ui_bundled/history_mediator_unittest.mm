// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_mediator.h"

#import <vector>

#import "base/functional/callback.h"
#import "components/history/core/browser/browsing_history_service.h"
#import "components/history/core/common/pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/history/ui_bundled/history_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake consumer recording history deletion policy state pushed by
// `HistoryMediator`.
@interface FakeHistoryConsumer : NSObject <HistoryConsumer>
@property(nonatomic, assign, getter=isHistoryDeletionAllowed)
    BOOL historyDeletionAllowed;
@property(nonatomic, assign) NSUInteger historyDeletionAllowedUpdateCount;
@end

@implementation FakeHistoryConsumer

- (void)
    historyQueryWasCompletedWithResults:
        (const std::vector<history::BrowsingHistoryService::HistoryEntry>&)
            results
                       queryResultsInfo:(const history::BrowsingHistoryService::
                                             QueryResultsInfo&)queryResultsInfo
                    continuationClosure:(base::OnceClosure)continuationClosure {
}

- (void)historyWasDeleted {
}

- (void)showNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)shouldShowNotice {
}

- (void)setHistoryDeletionAllowed:(BOOL)historyDeletionAllowed {
  _historyDeletionAllowed = historyDeletionAllowed;
  _historyDeletionAllowedUpdateCount++;
}

@end

namespace {

class HistoryMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAllowDeletingBrowserHistory, true);
    consumer_ = [[FakeHistoryConsumer alloc] init];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  TestFaviconLoader favicon_loader_;
  TestingPrefServiceSimple pref_service_;
  HistoryMediator* mediator_ = nil;
  FakeHistoryConsumer* consumer_ = nil;
};

// Tests that setting the consumer pushes the initial
// `kAllowDeletingBrowserHistory` preference state.
TEST_F(HistoryMediatorTest, InitialConsumerPushReflectsPrefState) {
  pref_service_.SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  mediator_ = [[HistoryMediator alloc] initWithFaviconLoader:&favicon_loader_
                                                 prefService:&pref_service_];
  mediator_.consumer = consumer_;

  EXPECT_EQ(1U, consumer_.historyDeletionAllowedUpdateCount);
  EXPECT_FALSE(consumer_.historyDeletionAllowed);
}

// Tests that live updates to `kAllowDeletingBrowserHistory` push the updated
// state to the consumer.
TEST_F(HistoryMediatorTest, LivePrefUpdatesPushHistoryDeletionAllowedState) {
  mediator_ = [[HistoryMediator alloc] initWithFaviconLoader:&favicon_loader_
                                                 prefService:&pref_service_];
  mediator_.consumer = consumer_;
  EXPECT_EQ(1U, consumer_.historyDeletionAllowedUpdateCount);
  EXPECT_TRUE(consumer_.historyDeletionAllowed);

  pref_service_.SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
  EXPECT_EQ(2U, consumer_.historyDeletionAllowedUpdateCount);
  EXPECT_FALSE(consumer_.historyDeletionAllowed);

  pref_service_.SetBoolean(prefs::kAllowDeletingBrowserHistory, true);
  EXPECT_EQ(3U, consumer_.historyDeletionAllowedUpdateCount);
  EXPECT_TRUE(consumer_.historyDeletionAllowed);
}

// Tests that `-disconnect` stops pushing preference updates to the consumer.
TEST_F(HistoryMediatorTest, DisconnectStopsPushingPrefUpdates) {
  mediator_ = [[HistoryMediator alloc] initWithFaviconLoader:&favicon_loader_
                                                 prefService:&pref_service_];
  mediator_.consumer = consumer_;
  EXPECT_EQ(1U, consumer_.historyDeletionAllowedUpdateCount);
  EXPECT_TRUE(consumer_.historyDeletionAllowed);

  [mediator_ disconnect];

  pref_service_.SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
  EXPECT_EQ(1U, consumer_.historyDeletionAllowedUpdateCount);
  EXPECT_TRUE(consumer_.historyDeletionAllowed);
}

}  // namespace
