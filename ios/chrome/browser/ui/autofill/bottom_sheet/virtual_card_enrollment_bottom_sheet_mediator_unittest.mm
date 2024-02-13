// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface TestVirtualCardEnrollmentBottomSheetConsumer
    : NSObject <VirtualCardEnrollmentBottomSheetConsumer>

@property(nonatomic, strong) VirtualCardEnrollmentBottomSheetData* cardData;
@end

@implementation TestVirtualCardEnrollmentBottomSheetConsumer

@end

class VirtualCardEnrollmentBottomSheetMediatorTest : public PlatformTest {};

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest, SetsCardDataOnConsumer) {
  autofill::VirtualCardEnrollUiModel model;
  model.window_title = std::u16string(u"Title");
  model.explanatory_message =
      std::u16string(u"Explanatory message. Learn More");
  model.accept_action_text = std::u16string(u"Accept action");
  model.cancel_action_text = std::u16string(u"Cancel action");
  model.learn_more_link_text = std::u16string(u"Learn more");
  model.enrollment_fields.google_legal_message =
      std::vector<autofill::LegalMessageLine>({autofill::TestLegalMessageLine(
          /*ascii_text=*/"Google legal message",
          /*links=*/{
              autofill::LegalMessageLine::Link(
                  /*start=*/2, /*end=*/3, /*url_spec=*/"https://google.test"),
          })});
  model.enrollment_fields.issuer_legal_message =
      std::vector<autofill::LegalMessageLine>({autofill::TestLegalMessageLine(
          /*ascii_text=*/"Issuer legal message",
          /*links=*/{
              autofill::LegalMessageLine::Link(
                  /*start=*/4, /*end=*/9, /*url_spec=*/"https://issuer.test"),
          })});
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      [[VirtualCardEnrollmentBottomSheetMediator alloc] initWithUiModel:model];
  TestVirtualCardEnrollmentBottomSheetConsumer* consumer =
      [[TestVirtualCardEnrollmentBottomSheetConsumer alloc] init];

  [mediator setConsumer:consumer];

  VirtualCardEnrollmentBottomSheetData* data = consumer.cardData;
  EXPECT_TRUE(data != nil);
  EXPECT_TRUE([data.title isEqual:@"Title"]);
  EXPECT_TRUE(
      [data.explanatoryMessage isEqual:@"Explanatory message. Learn More"]);
  EXPECT_TRUE([data.acceptActionText isEqual:@"Accept action"]);
  EXPECT_TRUE([data.cancelActionText isEqual:@"Cancel action"]);
  EXPECT_TRUE([data.learnMoreLinkText isEqual:@"Learn more"]);
  EXPECT_EQ(1u, [data.paymentServerLegalMessageLines count]);
  for (SaveCardMessageWithLinks* line in data.paymentServerLegalMessageLines) {
    EXPECT_TRUE([line.messageText isEqual:@"Google legal message"]);
    EXPECT_TRUE([line.linkRanges
        isEqualToArray:@[ [NSValue valueWithRange:NSMakeRange(2, 1)] ]]);
    EXPECT_EQ(line.linkURLs, std::vector<GURL>({GURL("https://google.test")}));
  }
  EXPECT_EQ(1u, [data.issuerLegalMessageLines count]);
  for (SaveCardMessageWithLinks* line in data.issuerLegalMessageLines) {
    EXPECT_TRUE([line.messageText isEqual:@"Issuer legal message"]);
    EXPECT_TRUE([line.linkRanges
        isEqualToArray:@[ [NSValue valueWithRange:NSMakeRange(4, 5)] ]]);
    EXPECT_EQ(line.linkURLs, std::vector<GURL>({GURL("https://issuer.test")}));
  }
}
