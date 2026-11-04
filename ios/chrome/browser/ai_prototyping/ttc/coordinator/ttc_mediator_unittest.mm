// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class TtcMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kTTCEnabled);
    TTCKeyedServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    ttc_service_ = TTCKeyedServiceFactory::GetForProfile(profile_.get());
    mediator_ = [[TTCMediator alloc] initWithTTCService:ttc_service_];
    mock_consumer_ = OCMProtocolMock(@protocol(TTCConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    ttc_service_ = nullptr;
    mock_consumer_ = nil;
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TTCKeyedService> ttc_service_ = nullptr;
  TTCMediator* mediator_ = nil;
  id<TTCConsumer> mock_consumer_ = nil;
};

// Tests journal hydration, detailed mode toggle, component filtering, in-place
// updates, and clearing.
TEST_F(TtcMediatorTest, JournalHydrationFilteringAndClear) {
  ttc_service_->journal().Log(
      TTCJournalComponent::kSession, "TtcSessionLifecycle",
      TTCJournalDetailsBuilder().SetSummary("Session live").Build());
  ttc_service_->journal().Log(TTCJournalComponent::kBackend, "ClockSync");

  OCMExpect([mock_consumer_ setJournalLines:@[ @"Session live" ]]);
  mediator_.consumer = mock_consumer_;
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  // Toggling detailed mode shows both entries with component tags.
  OCMExpect([mock_consumer_
      setJournalLines:[OCMArg checkWithBlock:^BOOL(NSArray<NSString*>* lines) {
        if (lines.count != 2u) {
          return NO;
        }
        return [lines[0] containsString:@"[Session] TtcSessionLifecycle"] &&
               [lines[1] containsString:@"[Backend] ClockSync"];
      }]]);
  [mediator_ setJournalDetailedMode:YES];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  [mediator_ setJournalDetailedMode:NO];

  // Live append and in-place update.
  OCMExpect([mock_consumer_ appendJournalLine:@"Input: Hello"]);
  uint64_t transcript_id = ttc_service_->journal().Log(
      TTCJournalComponent::kConversation, "TtcInputTranscription",
      TTCJournalDetailsBuilder().SetSummary("Input: Hello").Build());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  OCMExpect([mock_consumer_
      setJournalLines:(@[ @"Session live", @"Input: Hello world" ])]);
  ttc_service_->journal().UpdateEntry(
      transcript_id,
      TTCJournalDetailsBuilder().SetSummary("Input: Hello world").Build());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  // Filter to Session only, then clear.
  OCMExpect([mock_consumer_ setJournalLines:@[ @"Session live" ]]);
  [mediator_ setJournalComponentFilter:TTCJournalFilter::kSession];
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  OCMExpect([mock_consumer_ setJournalLines:@[]]);
  [mediator_ clearJournal];
  EXPECT_TRUE(ttc_service_->journal().entries().empty());
  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling `viewWillAppear` triggers hydration of the consumer.
TEST_F(TtcMediatorTest, ViewWillAppearHydratesConsumer) {
  mediator_.consumer = mock_consumer_;

  ttc_service_->journal().Log(
      TTCJournalComponent::kSession, "TtcSessionLifecycle",
      TTCJournalDetailsBuilder().SetSummary("Session live").Build());

  OCMExpect([mock_consumer_ setJournalLines:@[ @"Session live" ]]);

  [mediator_ viewWillAppear];

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

// Tests that calling `disconnect` detaches the consumer and stops forwarding
// journal entry additions, updates, and clear events.
TEST_F(TtcMediatorTest, DisconnectStopsForwardingJournalEvents) {
  uint64_t entry_id = ttc_service_->journal().Log(
      TTCJournalComponent::kSession, "TtcSessionLifecycle",
      TTCJournalDetailsBuilder().SetSummary("Session live").Build());
  OCMExpect([mock_consumer_ setJournalLines:@[ @"Session live" ]]);
  mediator_.consumer = mock_consumer_;
  EXPECT_OCMOCK_VERIFY(mock_consumer_);

  [mediator_ disconnect];

  OCMReject([mock_consumer_ appendJournalLine:[OCMArg any]]);
  OCMReject([mock_consumer_ setJournalLines:[OCMArg any]]);

  ttc_service_->journal().Log(TTCJournalComponent::kSession,
                              "After disconnect");
  ttc_service_->journal().UpdateEntry(
      entry_id, TTCJournalDetailsBuilder().SetSummary("Updated").Build());
  ttc_service_->journal().Clear();

  EXPECT_OCMOCK_VERIFY(mock_consumer_);
}

}  // namespace
