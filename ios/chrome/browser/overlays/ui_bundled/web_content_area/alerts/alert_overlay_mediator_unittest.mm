// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/alerts/alert_overlay_mediator.h"

#import "base/functional/bind.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/test/fake_alert_consumer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response_info.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/shared/ui/elements/text_field_configuration.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace {
// Alert setup consts.
const size_t kButtonIndexOkRow = 1;
const size_t kButtonIndexOkCol = 0;
const size_t kTextFieldIndex = 0;

// Recorded when OK button is tapped.
const char kOKTappedUserActionName[] = "OKTappedUserActionName";

// Fake response for use in tests.
class FakeResponseInfo : public OverlayResponseInfo<FakeResponseInfo> {
 public:
  ~FakeResponseInfo() override {}

  // Whether the user tapped the OK button on the dialog.
  bool ok_button_tapped() const { return ok_button_tapped_; }
  // The text field input.
  NSString* input() const { return input_; }

 private:
  OVERLAY_USER_DATA_SETUP(FakeResponseInfo);
  FakeResponseInfo(bool ok_button_tapped, NSString* input)
      : ok_button_tapped_(ok_button_tapped), input_(input) {}

  const bool ok_button_tapped_ = false;
  NSString* const input_ = nil;
};
OVERLAY_USER_DATA_SETUP_IMPL(FakeResponseInfo);

// Creates an OverlayResponse with a FakeResponseInfo from one created with an
// AlertResponse.
std::unique_ptr<OverlayResponse> CreateFakeResponse(
    std::unique_ptr<OverlayResponse> alert_response) {
  AlertResponse* alert_info = alert_response->GetInfo<AlertResponse>();
  return OverlayResponse::CreateWithInfo<FakeResponseInfo>(
      alert_info->tapped_button_row_index() == kButtonIndexOkRow &&
          alert_info->tapped_button_column_index() == kButtonIndexOkCol,
      alert_info->text_field_values()[kTextFieldIndex]);
}

// Fake request for use in tests.
class FakeRequestConfig : public OverlayResponseInfo<FakeRequestConfig> {
 public:
  ~FakeRequestConfig() override {}

 private:
  OVERLAY_USER_DATA_SETUP(FakeRequestConfig);
  FakeRequestConfig() {}

  void CreateAuxiliaryData(base::SupportsUserData* user_data) override {
    // Creates an AlertRequest with an OK and Cancel button and a single
    // text field.
    NSArray<TextFieldConfiguration*>* text_field_configs = @[
      [[TextFieldConfiguration alloc]
                     initWithText:@""
                      placeholder:@""
          accessibilityIdentifier:@""
           autocapitalizationType:UITextAutocapitalizationTypeSentences
                  secureTextEntry:NO],
    ];
    const std::vector<std::vector<ButtonConfig>> button_configs{
        {ButtonConfig(@"First Row")},
        {ButtonConfig(@"OK", kOKTappedUserActionName),
         ButtonConfig(@"Cancel", UIAlertActionStyleCancel)}};
    AlertRequest::CreateForUserData(user_data, @"title", @"message",
                                    @"accessibility_identifier",
                                    text_field_configs, button_configs,
                                    base::BindRepeating(&CreateFakeResponse));
  }
};
OVERLAY_USER_DATA_SETUP_IMPL(FakeRequestConfig);
}  // namespace

#pragma mark - FakeAlertOverlayMediatorDataSource

// Fake AlertOverlayMediatorDataSource for use in tests.
@interface FakeAlertOverlayMediatorDataSource
    : NSObject <AlertOverlayMediatorDataSource>
// The text field values to return for the data source protocol.
@property(nonatomic, strong) NSArray<NSString*>* textFieldValues;
@end
@implementation FakeAlertOverlayMediatorDataSource
- (NSString*)textFieldInputForMediator:(AlertOverlayMediator*)mediator
                        textFieldIndex:(NSUInteger)index {
  return index < self.textFieldValues.count ? self.textFieldValues[index] : nil;
}
@end

#pragma mark - AlertOverlayMediatorTest

// Test fixture for AlertOverlayMediator.
class AlertOverlayMediatorTest : public PlatformTest {
 protected:
  AlertOverlayMediatorTest()
      : request_(OverlayRequest::CreateWithConfig<FakeRequestConfig>()),
        consumer_([[FakeAlertConsumer alloc] init]),
        mediator_(
            [[AlertOverlayMediator alloc] initWithRequest:request_.get()]) {
    mediator_.consumer = consumer_;
  }

  std::unique_ptr<OverlayRequest> request_;
  FakeAlertConsumer* consumer_ = nil;
  AlertOverlayMediator* mediator_ = nil;
};

// Tests that the AlertOverlayMediator's subclassing properties are correctly
// applied to the consumer.
TEST_F(AlertOverlayMediatorTest, SetUpConsumer) {
  AlertRequest* alert_request = request_->GetConfig<AlertRequest>();
  ASSERT_TRUE(alert_request);
  EXPECT_NSEQ(alert_request->title(), consumer_.title);
  EXPECT_NSEQ(alert_request->message(), consumer_.message);
  EXPECT_NSEQ(alert_request->accessibility_identifier(),
              consumer_.alertAccessibilityIdentifier);
  EXPECT_NSEQ(alert_request->text_field_configs(),
              consumer_.textFieldConfigurations);
  ASSERT_EQ(2U, alert_request->button_configs().size());
  ASSERT_EQ(1U, alert_request->button_configs()[0].size());
  ASSERT_EQ(2U, alert_request->button_configs()[1].size());
  size_t rows_count = alert_request->button_configs().size();
  for (size_t i = 0; i < rows_count; ++i) {
    NSArray<AlertAction*>* actions = consumer_.actions[i];
    for (size_t j = 0; j < [actions count]; ++j) {
      AlertAction* consumer_action = actions[j];
      const ButtonConfig& button_config = alert_request->button_configs()[i][j];
      EXPECT_NSEQ(button_config.title, consumer_action.title);
      EXPECT_EQ(button_config.style, consumer_action.style);
    }
  }
}

// Tests that AlertOverlayMediator successfully converts OverlayResponses
// created with AlertResponses into their feature-specific response.
TEST_F(AlertOverlayMediatorTest, ResponseConversion) {
  // Set up a fake datasource for the text field values.
  FakeAlertOverlayMediatorDataSource* data_source =
      [[FakeAlertOverlayMediatorDataSource alloc] init];
  data_source.textFieldValues = @[ @"TextFieldValue" ];
  mediator_.dataSource = data_source;

  // Simulate a tap on the OK button.
  AlertAction* ok_button_action =
      consumer_.actions[kButtonIndexOkRow][kButtonIndexOkCol];
  ASSERT_TRUE(ok_button_action.handler);
  ok_button_action.handler(ok_button_action);

  // Verify that the request's completion callback is a FakeResponseInfo with
  // the expected values.
  OverlayResponse* response =
      request_->GetCallbackManager()->GetCompletionResponse();
  ASSERT_TRUE(response);
  FakeResponseInfo* info = response->GetInfo<FakeResponseInfo>();
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->ok_button_tapped());
  EXPECT_NSEQ(data_source.textFieldValues[0], info->input());
}

// Tests UMA user action recording.
TEST_F(AlertOverlayMediatorTest, UserActionRecording) {
  // Tapping OK button records User Action.
  AlertAction* ok_button_action =
      consumer_.actions[kButtonIndexOkRow][kButtonIndexOkCol];
  base::UserActionTester user_action_tester;
  EXPECT_EQ(0, user_action_tester.GetActionCount(kOKTappedUserActionName));
  ok_button_action.handler(ok_button_action);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kOKTappedUserActionName));
}
