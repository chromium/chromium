// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"

#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model_test_api.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/gfx/image/image_skia.h"
#import "ui/gfx/image/image_unittest_util.h"

// Expected time delay between showing the confirmation and dismissing the
// virtual card enrollment prompt.
const base::TimeDelta kExpectedConfirmationDismissDelay = base::Seconds(1.5);

@interface TestVirtualCardEnrollmentBottomSheetConsumer
    : NSObject <VirtualCardEnrollmentBottomSheetConsumer>

@property(nonatomic, strong) VirtualCardEnrollmentBottomSheetData* cardData;
@end

@implementation TestVirtualCardEnrollmentBottomSheetConsumer

- (void)showLoadingState {
}

- (void)showConfirmationState {
}

@end

using autofill::test_api;

class VirtualCardEnrollmentBottomSheetMediatorTest : public PlatformTest {
 public:
  VirtualCardEnrollmentBottomSheetMediatorTest()
      : histogram_tester_(),
        fake_card_art_(gfx::test::CreateImageSkia(60, 24)),
        mock_browser_coordinator_handler_(
            OCMProtocolMock(@protocol(BrowserCoordinatorCommands))) {}

 protected:
  // Creates a virtual card enrollment model with required properties.
  std::unique_ptr<autofill::VirtualCardEnrollUiModel> MakeModel() {
    // The enrollment source cannot be kNone, so it is set to a default of
    // kDownstream here.
    autofill::VirtualCardEnrollmentFields enrollment_fields;
    enrollment_fields.virtual_card_enrollment_source =
        autofill::VirtualCardEnrollmentSource::kDownstream;
    std::unique_ptr<autofill::VirtualCardEnrollUiModel> model =
        std::make_unique<autofill::VirtualCardEnrollUiModel>(enrollment_fields);
    return model;
  }

  // Creates the mediator with the given model and mock callbacks and mock
  // commands.
  VirtualCardEnrollmentBottomSheetMediator* MakeMediator(
      std::unique_ptr<autofill::VirtualCardEnrollUiModel> model) {
    model_ = model->GetWeakPtr();
    return [[VirtualCardEnrollmentBottomSheetMediator alloc]
                  initWithUIModel:std::move(model)
                        callbacks:MakeFakeCallbacks()
        browserCoordinatorHandler:mock_browser_coordinator_handler_];
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
  id<BrowserCoordinatorCommands> mock_browser_coordinator_handler_;
  VirtualCardEnrollmentBottomSheetMediator* mediator_;
  base::WeakPtr<autofill::VirtualCardEnrollUiModel> model_;
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest, SetsCardDataOnConsumer) {
  std::unique_ptr<autofill::VirtualCardEnrollUiModel> model(MakeModel());
  test_api(*model).window_title() = std::u16string(u"Title");
  test_api(*model).explanatory_message() =
      std::u16string(u"Explanatory message. Learn More");
  test_api(*model).accept_action_text() = std::u16string(u"Accept action");
  test_api(*model).cancel_action_text() = std::u16string(u"Cancel action");
  test_api(*model).learn_more_link_text() = std::u16string(u"Learn more");
  test_api(*model).enrollment_fields().google_legal_message =
      std::vector<autofill::LegalMessageLine>({autofill::TestLegalMessageLine(
          /*ascii_text=*/"Google legal message",
          /*links=*/{
              autofill::LegalMessageLine::Link(
                  /*start=*/2, /*end=*/3, /*url_spec=*/"https://google.test"),
          })});
  test_api(*model).enrollment_fields().issuer_legal_message =
      std::vector<autofill::LegalMessageLine>({autofill::TestLegalMessageLine(
          /*ascii_text=*/"Issuer legal message",
          /*links=*/{
              autofill::LegalMessageLine::Link(
                  /*start=*/4, /*end=*/9, /*url_spec=*/"https://issuer.test"),
          })});
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(std::move(model));
  TestVirtualCardEnrollmentBottomSheetConsumer* consumer =
      [[TestVirtualCardEnrollmentBottomSheetConsumer alloc] init];

  [mediator setConsumer:consumer];

  VirtualCardEnrollmentBottomSheetData* data = consumer.cardData;
  EXPECT_TRUE(data != nil);
  EXPECT_NSEQ(data.title, @"Title");
  EXPECT_NSEQ(data.explanatoryMessage, @"Explanatory message. Learn More");
  EXPECT_NSEQ(data.acceptActionText, @"Accept action");
  EXPECT_NSEQ(data.cancelActionText, @"Cancel action");
  EXPECT_NSEQ(data.learnMoreLinkText, @"Learn more");
  EXPECT_EQ(1u, [data.paymentServerLegalMessageLines count]);
  for (SaveCardMessageWithLinks* line in data.paymentServerLegalMessageLines) {
    EXPECT_NSEQ(line.messageText, @"Google legal message");
    EXPECT_NSEQ(line.linkRanges,
                @[ [NSValue valueWithRange:NSMakeRange(2, 1)] ]);
    EXPECT_EQ(line.linkURLs, std::vector<GURL>({GURL("https://google.test")}));
  }
  EXPECT_EQ(1u, [data.issuerLegalMessageLines count]);
  for (SaveCardMessageWithLinks* line in data.issuerLegalMessageLines) {
    EXPECT_NSEQ(line.messageText, @"Issuer legal message");
    EXPECT_NSEQ(line.linkRanges,
                @[ [NSValue valueWithRange:NSMakeRange(4, 5)] ]);
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
  std::unique_ptr<autofill::VirtualCardEnrollUiModel> model(MakeModel());
  test_api(*model).enrollment_fields().card_art_image = &fake_card_art_;
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(std::move(model));

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

  OCMExpect([mock_browser_coordinator_handler_
      dismissVirtualCardEnrollmentBottomSheet]);

  [mediator didAccept];

  EXPECT_OCMOCK_VERIFY((id)mock_browser_coordinator_handler_);
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

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       AcceptButtonPushedLogsLoadingViewNotShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  [mediator didAccept];

  // Expect 1 sample with `is_shown` (sample) being false.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.LoadingShown", /*sample=*/false,
      /*expected_count=*/1);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       AcceptButtonPushedLogsLoadingViewShown) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  [mediator didAccept];

  // Expect 1 sample with `is_shown` (sample) being true.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.LoadingShown", /*sample=*/true,
      /*expected_count=*/1);
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

  OCMExpect([mock_browser_coordinator_handler_
      dismissVirtualCardEnrollmentBottomSheet]);

  [mediator didCancel];

  EXPECT_OCMOCK_VERIFY((id)mock_browser_coordinator_handler_);
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

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       ShowsConfirmationWhenEnrolled) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  id<VirtualCardEnrollmentBottomSheetConsumer> mock_consumer =
      OCMProtocolMock(@protocol(VirtualCardEnrollmentBottomSheetConsumer));
  VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());
  mediator.consumer = mock_consumer;

  OCMExpect([mock_consumer showConfirmationState]);

  model_->SetEnrollmentProgress(
      autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled);

  EXPECT_OCMOCK_VERIFY((id)mock_consumer);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       LogsConfirmationShownWhenEnrolled) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  // Hold a strong reference to the mediator during the duration of the test.
  [[maybe_unused]] VirtualCardEnrollmentBottomSheetMediator* mediator =
      MakeMediator(MakeModel());

  model_->SetEnrollmentProgress(
      autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled);

  // Expect 1 sample with `is_shown` (sample) being true.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.ConfirmationShown.CardEnrolled",
      /*sample=*/true,
      /*expected_count=*/1);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       DelayAfterShowingConfirmation) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  // Hold a strong reference to the mediator during the duration of the test.
  [[maybe_unused]] VirtualCardEnrollmentBottomSheetMediator* unused_mediator =
      MakeMediator(MakeModel());
  model_->SetEnrollmentProgress(
      autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled);

  // Do not dismiss before the delay.
  OCMReject([mock_browser_coordinator_handler_
      dismissVirtualCardEnrollmentBottomSheet]);
  task_env_.FastForwardBy(kExpectedConfirmationDismissDelay -
                          base::Milliseconds(1));

  EXPECT_OCMOCK_VERIFY((id)mock_browser_coordinator_handler_);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       DismissAfterConfirmationAndDelay) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  // Hold a strong reference to the mediator during the duration of the test.
  [[maybe_unused]] VirtualCardEnrollmentBottomSheetMediator* unused_mediator =
      MakeMediator(MakeModel());
  model_->SetEnrollmentProgress(
      autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled);

  // Dismiss after the delay.
  OCMExpect([mock_browser_coordinator_handler_
      dismissVirtualCardEnrollmentBottomSheet]);
  task_env_.FastForwardBy(kExpectedConfirmationDismissDelay);

  EXPECT_OCMOCK_VERIFY((id)mock_browser_coordinator_handler_);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       DismissesWhenEnrollmentFailed) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  // Hold a strong reference to the mediator during the duration of the test.
  [[maybe_unused]] VirtualCardEnrollmentBottomSheetMediator* unused_mediator =
      MakeMediator(MakeModel());

  OCMExpect([mock_browser_coordinator_handler_
      dismissVirtualCardEnrollmentBottomSheet]);

  model_->SetEnrollmentProgress(
      autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kFailed);

  EXPECT_OCMOCK_VERIFY((id)mock_browser_coordinator_handler_);
}

TEST_F(VirtualCardEnrollmentBottomSheetMediatorTest,
       LogsConfirmationShownWhenEnrollmentFailed) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  // Hold a strong reference to the mediator during the duration of the test.
  [[maybe_unused]] VirtualCardEnrollmentBottomSheetMediator* unused_mediator =
      MakeMediator(MakeModel());

  model_->SetEnrollmentProgress(
      autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kFailed);

  // Expect 1 sample with `is_shown` (sample) being true.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.ConfirmationShown.CardNotEnrolled",
      /*sample=*/true,
      /*expected_count=*/1);
}
