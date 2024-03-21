// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_observer_bridge.h"

#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_observer.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

@interface FakeAutofillBottomSheetObserving
    : NSObject <AutofillBottomSheetObserving>

- (autofill::FormActivityParams)params;

@end

@implementation FakeAutofillBottomSheetObserving {
  autofill::FormActivityParams _params;
}

- (autofill::FormActivityParams)params {
  return _params;
}

- (void)willShowPaymentsBottomSheetWithParams:
    (const autofill::FormActivityParams&)params {
  _params = params;
}

@end

// Test fixture to test AutofillBottomSheetObserverBridge class.
class AutofillBottomSheetObserverBridgeTest : public PlatformTest {
 protected:
  AutofillBottomSheetObserverBridgeTest() {
    observer_ = [[FakeAutofillBottomSheetObserving alloc] init];

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web::ContentWorld content_world =
        AutofillBottomSheetJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    fake_web_state_.SetWebFramesManager(content_world,
                                        std::move(frames_manager));

    AutofillBottomSheetTabHelper::CreateForWebState(&fake_web_state_);
    AutofillBottomSheetTabHelper* helper =
        AutofillBottomSheetTabHelper::FromWebState(&fake_web_state_);

    observer_bridge_ =
        std::make_unique<autofill::AutofillBottomSheetObserverBridge>(observer_,
                                                                      helper);
  }
  web::FakeWebState fake_web_state_;
  FakeAutofillBottomSheetObserving* observer_;
  std::unique_ptr<autofill::AutofillBottomSheetObserverBridge> observer_bridge_;
};

// Tests willShowPaymentsBottomSheetWithParams: forwarding.
TEST_F(AutofillBottomSheetObserverBridgeTest, TestShowPaymentsBottomSheet) {
  // Params values are empty.
  EXPECT_EQ("", [observer_ params].form_name);
  EXPECT_EQ("", [observer_ params].field_type);
  EXPECT_EQ("", [observer_ params].type);

  std::string form_name = "form-name";
  std::string field_type = "text";
  std::string type = "focus";

  autofill::FormActivityParams params;
  params.form_name = form_name;
  params.field_type = field_type;
  params.type = type;

  observer_bridge_->WillShowPaymentsBottomSheet(params);

  // Params values are filled properly.
  EXPECT_EQ(form_name, [observer_ params].form_name);
  EXPECT_EQ(field_type, [observer_ params].field_type);
  EXPECT_EQ(type, [observer_ params].type);
}
