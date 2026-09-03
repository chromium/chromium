// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_params.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_lottie_params.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Test constants.
NSString* const kSemanticColorKey = @"grouped_primary_background_color";
NSString* const kSemanticColorValue = @"grouped_primary_background_color";
NSString* const kCustomColorKey = @"custom_color";
NSString* const kLightCustomHexColor = @"#123456";
NSString* const kDarkCustomHexColor = @"0x654321";
NSString* const kCustomHexHashKey = @"custom_hex_hash";
NSString* const kCustomHexHashValue = @"#112233";
NSString* const kCustomHexPrefixKey = @"custom_hex_prefix";
NSString* const kCustomHexPrefixValue = @"0x445566";
NSString* const kTextKey = @"text_key";
NSString* const kReplacementText = @"Replacement text";
NSString* const kTextLayerKey = @"text_layer";
NSString* const kTestText = @"Test Text";
NSString* const kImageURLWithExtension = @"docking_promo.json";
NSString* const kImageURLWithoutExtension = @"docking_promo";
NSString* const kNonexistentImage = @"nonexistent_promo_image";
NSString* const kAnimationName = @"docking_promo";
NSString* const kLightColorHexA = @"#AABBCC";
NSString* const kDarkColorHexC = @"#DDEEFF";
NSString* const kInvalidColor = @"invalid_color";
NSString* const kTestTitle = @"Test Title";
NSString* const kTestBody = @"Test Body";
NSString* const kTestPrimaryAction = @"Primary Action";
NSString* const kTestSecondaryAction = @"Secondary Action";

// Creates standard test custom UI params with default test strings.
BackendPromoCustomUIParams* CreateCustomUIParams(
    NSString* image_url = nil,
    NSArray<NSString*>* instruction_steps = nil) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = kTestTitle;
  params.body = kTestBody;
  params.primaryActionTitle = kTestPrimaryAction;
  params.secondaryActionTitle = kTestSecondaryAction;
  params.imageURL = image_url;
  params.instructionSteps = instruction_steps;
  return params;
}

// Creates standard test Lottie params.
BackendPromoLottieParams* CreateLottieParams(
    NSDictionary<NSString*, NSString*>* light_color_mapping = nil,
    NSDictionary<NSString*, NSString*>* dark_color_mapping = nil,
    NSDictionary<NSString*, NSString*>* text_mapping = nil) {
  BackendPromoLottieParams* lottie_params =
      [[BackendPromoLottieParams alloc] init];
  lottie_params.lightColorMapping = light_color_mapping;
  lottie_params.darkColorMapping = dark_color_mapping;
  lottie_params.textMapping = text_mapping;
  return lottie_params;
}

// Creates an initialized and view-loaded BackendPromoCustomUIViewController.
BackendPromoCustomUIViewController* CreateLoadedViewController(
    BackendPromoLottieParams* lottie_params = nil,
    NSString* image_url = kImageURLWithoutExtension) {
  BackendPromoCustomUIParams* params = CreateCustomUIParams(image_url);
  params.lottieParams = lottie_params;
  BackendPromoCustomUIViewController* custom_view_controller =
      [[BackendPromoCustomUIViewController alloc] initWithParams:params];
  [custom_view_controller loadViewIfNeeded];
  return custom_view_controller;
}

using BackendPromoCustomUIViewControllerTest = PlatformTest;

// Tests that BackendPromoLottieParams and BackendPromoCustomUIParams properly
// store and read parameters.
TEST_F(BackendPromoCustomUIViewControllerTest, TestParamsLottieParams) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  EXPECT_EQ(params.lottieParams, nil);

  BackendPromoLottieParams* lottieParams =
      [[BackendPromoLottieParams alloc] init];
  lottieParams.lightColorMapping = @{
    kSemanticColorKey : kSemanticColorValue,
    kCustomColorKey : kLightCustomHexColor,
  };
  lottieParams.darkColorMapping = @{
    kSemanticColorKey : kSemanticColorValue,
    kCustomColorKey : kDarkCustomHexColor,
  };
  lottieParams.textMapping = @{
    kTextKey : kReplacementText,
  };

  params.lottieParams = lottieParams;

  EXPECT_NSEQ(params.lottieParams, lottieParams);
  EXPECT_NSEQ(params.lottieParams.lightColorMapping[kCustomColorKey],
              kLightCustomHexColor);
  EXPECT_NSEQ(params.lottieParams.darkColorMapping[kCustomColorKey],
              kDarkCustomHexColor);
  EXPECT_NSEQ(params.lottieParams.textMapping[kTextKey], kReplacementText);
}

// Tests that viewControllerWithParams returns a
// BackendPromoCustomUIViewController when a matching Lottie animation
// resource is found in the main bundle.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestViewControllerWithParamsReturnsAnimatedViewController) {
  id handler = OCMProtocolMock(@protocol(ConfirmationAlertActionHandler));
  BackendPromoCustomUIParams* params =
      CreateCustomUIParams(kImageURLWithoutExtension);

  UIViewController* viewController =
      [BackendPromoCustomUIViewController viewControllerWithParams:params
                                                     actionHandler:handler];

  ASSERT_TRUE([viewController
      isKindOfClass:[BackendPromoCustomUIViewController class]]);
  BackendPromoCustomUIViewController* animatedVC =
      base::apple::ObjCCastStrict<BackendPromoCustomUIViewController>(
          viewController);
  EXPECT_EQ(animatedVC.actionHandler, handler);
}

// Tests that viewControllerWithParams returns a
// ConfirmationAlertViewController when no matching Lottie animation
// resource exists.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestViewControllerWithParamsReturnsConfirmationAlertViewController) {
  id handler = OCMProtocolMock(@protocol(ConfirmationAlertActionHandler));
  BackendPromoCustomUIParams* params = CreateCustomUIParams(kNonexistentImage);

  UIViewController* viewController =
      [BackendPromoCustomUIViewController viewControllerWithParams:params
                                                     actionHandler:handler];

  ASSERT_TRUE(
      [viewController isKindOfClass:[ConfirmationAlertViewController class]]);
  ConfirmationAlertViewController* alertVC =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          viewController);
  EXPECT_NSEQ(alertVC.titleString, kTestTitle);
  EXPECT_NSEQ(alertVC.subtitleString, kTestBody);
  EXPECT_NSEQ(alertVC.configuration.primaryActionString, kTestPrimaryAction);
  EXPECT_NSEQ(alertVC.configuration.secondaryActionString,
              kTestSecondaryAction);
  EXPECT_EQ(alertVC.actionHandler, handler);
}

// Tests that viewControllerWithParams configures InstructionView on
// underTitleView and clears subtitleString for animated promos.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestViewControllerWithParamsAnimatedPromoInstructionSteps) {
  id handler = OCMProtocolMock(@protocol(ConfirmationAlertActionHandler));
  BackendPromoCustomUIParams* params = CreateCustomUIParams(
      kImageURLWithoutExtension, @[ @"Step 1", @"Step 2" ]);

  UIViewController* viewController =
      [BackendPromoCustomUIViewController viewControllerWithParams:params
                                                     actionHandler:handler];

  ASSERT_TRUE([viewController
      isKindOfClass:[BackendPromoCustomUIViewController class]]);
  BackendPromoCustomUIViewController* animatedVC =
      base::apple::ObjCCastStrict<BackendPromoCustomUIViewController>(
          viewController);
  EXPECT_NE(animatedVC.underTitleView, nil);
  EXPECT_TRUE(
      [animatedVC.underTitleView isKindOfClass:[InstructionView class]]);
  EXPECT_EQ(animatedVC.subtitleString, nil);
}

// Tests that viewControllerWithParams configures InstructionView on
// underTitleView and clears subtitleString for static promos.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestViewControllerWithParamsStaticPromoInstructionSteps) {
  id handler = OCMProtocolMock(@protocol(ConfirmationAlertActionHandler));
  BackendPromoCustomUIParams* params =
      CreateCustomUIParams(kNonexistentImage, @[ @"Step 1", @"Step 2" ]);

  UIViewController* viewController =
      [BackendPromoCustomUIViewController viewControllerWithParams:params
                                                     actionHandler:handler];

  ASSERT_TRUE(
      [viewController isKindOfClass:[ConfirmationAlertViewController class]]);
  ConfirmationAlertViewController* alertVC =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          viewController);
  EXPECT_NE(alertVC.underTitleView, nil);
  EXPECT_TRUE([alertVC.underTitleView isKindOfClass:[InstructionView class]]);
  EXPECT_EQ(alertVC.subtitleString, nil);
}

// Tests that BackendPromoCustomUIViewController correctly initializes string
// properties from BackendPromoCustomUIParams.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestInitWithParamsPopulatesStrings) {
  BackendPromoCustomUIParams* params = CreateCustomUIParams();

  BackendPromoCustomUIViewController* customViewController =
      [[BackendPromoCustomUIViewController alloc] initWithParams:params];

  EXPECT_NSEQ(customViewController.titleString, kTestTitle);
  EXPECT_NSEQ(customViewController.subtitleString, kTestBody);
  EXPECT_NSEQ(customViewController.primaryActionString, kTestPrimaryAction);
  EXPECT_NSEQ(customViewController.secondaryActionString, kTestSecondaryAction);
}

// Tests that BackendPromoCustomUIViewController correctly configures
// underTitleView and clears subtitleString when instructionSteps are provided.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestInitWithParamsInstructionSteps) {
  BackendPromoCustomUIParams* params =
      CreateCustomUIParams(nil, @[ @"Step 1", @"Step 2" ]);

  BackendPromoCustomUIViewController* customViewController =
      [[BackendPromoCustomUIViewController alloc] initWithParams:params];

  EXPECT_NE(customViewController.underTitleView, nil);
  EXPECT_TRUE([customViewController.underTitleView
      isKindOfClass:[InstructionView class]]);
  EXPECT_EQ(customViewController.subtitleString, nil);
}

// Tests that BackendPromoCustomUIViewController correctly configures its
// animation name, light and dark color providers, and text provider.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestAnimatedViewControllerLottieSetup) {
  BackendPromoLottieParams* lottieParams =
      CreateLottieParams(@{
        kSemanticColorKey : kSemanticColorValue,
        kCustomHexHashKey : kCustomHexHashValue,
      },
                         @{
                           kSemanticColorKey : kSemanticColorValue,
                           kCustomHexPrefixKey : kCustomHexPrefixValue,
                         },
                         @{
                           kTextLayerKey : kTestText,
                         });
  BackendPromoCustomUIViewController* customViewController =
      CreateLoadedViewController(lottieParams, kImageURLWithExtension);

  EXPECT_NSEQ(customViewController.animationName, kAnimationName);
  EXPECT_FALSE(customViewController.useLegacyDarkMode);
  EXPECT_NE(customViewController.lightModeColorProvider, nil);
  EXPECT_NE(customViewController.darkModeColorProvider, nil);
  EXPECT_NSEQ(customViewController.animationTextProvider[kTextLayerKey],
              kTestText);
  EXPECT_NE(customViewController.lightModeColorProvider[kCustomHexHashKey],
            nil);
  EXPECT_NE(customViewController.darkModeColorProvider[kCustomHexPrefixKey],
            nil);
}

// Tests that darkModeColorProvider falls back to lightModeColorProvider when
// darkColorMapping is empty.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestDarkModeFallsBackToLightMode) {
  BackendPromoCustomUIViewController* customViewController =
      CreateLoadedViewController(
          CreateLottieParams(@{kCustomColorKey : kLightColorHexA}));

  EXPECT_NE(customViewController.lightModeColorProvider, nil);
  EXPECT_NE(customViewController.darkModeColorProvider, nil);
  EXPECT_EQ(customViewController.darkModeColorProvider,
            customViewController.lightModeColorProvider);
  EXPECT_NE(customViewController.darkModeColorProvider[kCustomColorKey], nil);
  EXPECT_NSEQ(customViewController.darkModeColorProvider[kCustomColorKey],
              customViewController.lightModeColorProvider[kCustomColorKey]);
}

// Tests that darkModeColorProvider falls back to lightModeColorProvider when
// darkColorMapping contains invalid colors and ColorProviderFromMapping returns
// nil.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestDarkModeFallsBackWhenDarkColorMappingInvalid) {
  BackendPromoCustomUIViewController* customViewController =
      CreateLoadedViewController(
          CreateLottieParams(@{kCustomColorKey : kLightColorHexA},
                             @{kCustomColorKey : kInvalidColor}));

  EXPECT_NE(customViewController.lightModeColorProvider, nil);
  EXPECT_NE(customViewController.darkModeColorProvider, nil);
  EXPECT_EQ(customViewController.darkModeColorProvider,
            customViewController.lightModeColorProvider);
  EXPECT_NSEQ(customViewController.darkModeColorProvider[kCustomColorKey],
              customViewController.lightModeColorProvider[kCustomColorKey]);
}

// Tests that lightModeColorProvider falls back to darkModeColorProvider when
// lightColorMapping is nil or invalid, but darkColorMapping is valid.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestLightModeFallsBackWhenLightColorMappingInvalid) {
  BackendPromoCustomUIViewController* customViewController =
      CreateLoadedViewController(
          CreateLottieParams(@{kCustomColorKey : kInvalidColor},
                             @{kCustomColorKey : kDarkColorHexC}));

  EXPECT_NE(customViewController.lightModeColorProvider, nil);
  EXPECT_NE(customViewController.darkModeColorProvider, nil);
  EXPECT_EQ(customViewController.lightModeColorProvider,
            customViewController.darkModeColorProvider);
  EXPECT_NE(customViewController.lightModeColorProvider[kCustomColorKey], nil);
}

// Tests that missing keys in one mode fall back to the other mode when color
// mappings have partially overlapping keys.
TEST_F(BackendPromoCustomUIViewControllerTest,
       TestColorProviderPartialOverlapKeyFallback) {
  BackendPromoCustomUIViewController* customViewController =
      CreateLoadedViewController(
          CreateLottieParams(@{kCustomHexHashKey : kCustomHexHashValue},
                             @{kCustomHexPrefixKey : kCustomHexPrefixValue}));

  // Both providers should contain both keys (the complete union of keys).
  EXPECT_NE(customViewController.lightModeColorProvider[kCustomHexHashKey],
            nil);
  EXPECT_NE(customViewController.lightModeColorProvider[kCustomHexPrefixKey],
            nil);
  EXPECT_NE(customViewController.darkModeColorProvider[kCustomHexHashKey], nil);
  EXPECT_NE(customViewController.darkModeColorProvider[kCustomHexPrefixKey],
            nil);

  // The missing dark color for `kCustomHexHashKey` fell back to light color.
  EXPECT_NSEQ(customViewController.darkModeColorProvider[kCustomHexHashKey],
              customViewController.lightModeColorProvider[kCustomHexHashKey]);

  // The missing light color for `kCustomHexPrefixKey` fell back to dark color.
  EXPECT_NSEQ(customViewController.lightModeColorProvider[kCustomHexPrefixKey],
              customViewController.darkModeColorProvider[kCustomHexPrefixKey]);
}

}  // namespace
