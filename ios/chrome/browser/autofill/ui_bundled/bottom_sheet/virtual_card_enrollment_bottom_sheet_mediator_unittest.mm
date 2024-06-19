// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"

#import "base/functional/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_consumer.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/gfx/image/image_skia.h"
#import "ui/gfx/image/image_unittest_util.h"

@interface TestVirtualCardEnrollmentBottomSheetConsumer
    : NSObject <VirtualCardEnrollmentBottomSheetConsumer>

@property(nonatomic, strong) VirtualCardEnrollmentBottomSheetData* cardData;
@end

@implementation TestVirtualCardEnrollmentBottomSheetConsumer

- (void)showLoadingState {
}

@end

class VirtualCardEnrollmentBottomSheetMediatorTest : public PlatformTest {
 public:
  VirtualCardEnrollmentBottomSheetMediatorTest()
      : histogram_tester_(),
        fake_card_art_(gfx::test::CreateImageSkia(60, 24)),
        mock_commands_(OCMProtocolMock(@protocol(BrowserCoordinatorCommands))) {
  }

 protected:
  // Creates a virtual card enrollment model with required properties.
  autofill::VirtualCardEnrollUiModel MakeModel() {
    autofill::VirtualCardEnrollUiModel model;
    // The enrollment source cannot be kNone, so it is set to a default of
    // kDownstream here.
    model.enrollment_fields.virtual_card_enrollment_source =
        autofill::VirtualCardEnrollmentSource::kDownstream;
    return model;
  }

  // Creates the mediator with the given model and mock callbacks and mock
  // commands.
  VirtualCardEnrollmentBottomSheetMediator* MakeMediator(
      autofill::VirtualCardEnrollUiModel model) {
    return [[VirtualCardEnrollmentBottomSheetMediator alloc]
                   initWithUiModel:model
                         callbacks:MakeFakeCallbacks()
        browserCoordinatorCommands:mock_commands_];
  }

  autofill::VirtualCardEnrollmentCallbacks MakeFakeCallbacks() {
    return autofill::VirtualCardEnrollmentCallbacks(
        base::BindOnce(
            &VirtualCardEnrollmentBottomSheetMediatorTest::OnAccepted,
            base::Unretained(this)),
        base::BindOnce(
            &VirtualCardEnrollmentBottomSheetMediatorTest::OnDeclined,
            base::Unretained(this)));
  }

  // Mock methods for VirtualCardEnrollmentCallbacks.
  MOCK_METHOD(void, OnAccepted, ());
  MOCK_METHOD(void, OnDeclined, ());

  base::HistogramTester histogram_tester_;
  gfx::ImageSkia fake_card_art_;
  id<BrowserCoordinatorCommands> mock_commands_;
};

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest, SetsCardDataOnConsumer) {
  autofill::VirtualCardEnrollUiModel model(MakeModel());
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
  VirtualCardEnrollmentBottomSheetMediator* mediator = MakeMediator(model);
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

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest, LogsShownMetric) {
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  [mediator setConsumer:nil];

  // Expect 1 sample with `is_reshow` (sample) being false.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Shown.Downstream", /*sample=*/0,
      /*expected_count=*/1);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       LogsCardArtAvailableMetric) {
  autofill::VirtualCardEnrollUiModel model(MakeModel());
  model.enrollment_fields.card_art_image = &fake_card_art_;
  VirtualCardEnrollmentBottomSheetMediator* mediator = MakeMediator(model);

  [mediator setConsumer:nil];

  // Expect 1 sample when `card_art_available` (sample) is true
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnroll.CardArtImageAvailable.Downstream",
      /*sample=*/1, /*expected_count=*/1);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       LogsCardArtNotAvailableMetric) {
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  [mediator setConsumer:nil];

  // Expect 1 sample when `card_art_available` (sample) is false.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnroll.CardArtImageAvailable.Downstream",
      /*sample=*/0, /*expected_count=*/1);
}

// Test that pushing accept calls the provided callback.
TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       AcceptButtonPushedExecutesCallback) {
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  EXPECT_CALL(*this, OnAccepted());
  EXPECT_CALL(*this, OnDeclined()).Times(0);

  [mediator didAccept];
}

// Test that pushing accept calls the browser coordinator to dismiss virtual
// card enrollment.
TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       AcceptButtonPushedDismissesVirtualCardEnrollment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  OCMExpect([mock_commands_ dismissVirtualCardEnrollmentBottomSheet]);

  [mediator didAccept];

  EXPECT_OCMOCK_VERIFY((id)mock_commands_);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       AcceptButtonPushedEntersLoadingState) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  id<VirtualCardEnrollmentBottomSheetConsumer> mock_consumer =
      OCMProtocolMock(@protocol(VirtualCardEnrollmentBottomSheetConsumer));
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());
  mediator.consumer = mock_consumer;

  OCMExpect([mock_consumer showLoadingState]);

  [mediator didAccept];

  EXPECT_OCMOCK_VERIFY((id)mock_consumer);
}

// Test that the result metric is logged when the prompt is accepted.
TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest, LogsAcceptedMetric) {
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  [mediator didAccept];

  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Downstream.FirstShow",
      autofill::VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      /*expected_count=*/1);
}

// Test that using the secondary button calls the provided decline callback.
TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       CancelButtonPushedExecutesCallback) {
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  EXPECT_CALL(*this, OnAccepted()).Times(0);
  EXPECT_CALL(*this, OnDeclined());

  [mediator didCancel];
}

// Test that pushing cancel calls the browser coordinator to dismiss virtual
// card enrollment.
TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       CancelButtonPushedDismissesVirtualCardEnrollment) {
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  OCMExpect([mock_commands_ dismissVirtualCardEnrollmentBottomSheet]);

  [mediator didCancel];

  EXPECT_OCMOCK_VERIFY((id)mock_commands_);
}

// Test that the result metric is logged when the prompt is accepted.
TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest, LogsCancelledMetric) {
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  [mediator didCancel];

  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Downstream.FirstShow",
      autofill::VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED,
      /*expected_count=*/1);
}
