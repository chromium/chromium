// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_view_controller.h"

#import "base/apple/bundle_locations.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_params.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_lottie_params.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"

namespace {

// Accessibility identifier for the backend promo custom UI view.
NSString* const kBackendPromoCustomUIViewAXID =
    @"kBackendPromoCustomUIViewAXID";

// Parses a hex color string (e.g. @"#123456", @"0x123456", or @"123456") into a
// UIColor.
UIColor* ColorFromHexString(NSString* hex_color_string) {
  if (!hex_color_string || hex_color_string.length == 0) {
    return nil;
  }
  NSString* clean_string = [hex_color_string
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if ([clean_string hasPrefix:@"#"]) {
    clean_string = [clean_string substringFromIndex:1];
  } else if ([clean_string hasPrefix:@"0x"] || [clean_string hasPrefix:@"0X"]) {
    clean_string = [clean_string substringFromIndex:2];
  }
  if (clean_string.length != 6) {
    return nil;
  }
  uint32_t rgb_value = 0;
  if (base::HexStringToUInt(base::SysNSStringToUTF8(clean_string),
                            &rgb_value)) {
    return UIColorFromRGB(rgb_value);
  }
  return nil;
}

// Returns a dictionary mapping Lottie color keypaths to UIColors based on the
// provided color mapping dictionary (supports semantic color names and hex
// strings).
NSDictionary<NSString*, UIColor*>* ColorProviderFromMapping(
    NSDictionary<NSString*, NSString*>* color_mapping) {
  if (!color_mapping || color_mapping.count == 0) {
    return nil;
  }
  NSMutableDictionary<NSString*, UIColor*>* provider =
      [NSMutableDictionary dictionaryWithCapacity:color_mapping.count];
  for (NSString* keypath in color_mapping) {
    NSString* proto_color = color_mapping[keypath];
    UIColor* custom_hex_color = ColorFromHexString(proto_color);
    if (custom_hex_color) {
      provider[keypath] = custom_hex_color;
    } else {
      UIColor* semantic_color = [UIColor colorNamed:proto_color];
      if (semantic_color) {
        provider[keypath] = semantic_color;
      }
    }
  }
  return provider.count > 0 ? [provider copy] : nil;
}

// Merges `primary` with `fallback` so that any key missing in `primary`
// inherits the color from `fallback`.
NSDictionary<NSString*, UIColor*>* ResolveColorProvider(
    NSDictionary<NSString*, UIColor*>* primary,
    NSDictionary<NSString*, UIColor*>* fallback) {
  if (!primary && !fallback) {
    return nil;
  }
  if (!fallback) {
    return primary;
  }
  if (!primary) {
    return fallback;
  }
  if ([primary count] == [fallback count] &&
      [[NSSet setWithArray:primary.allKeys]
          isEqualToSet:[NSSet setWithArray:fallback.allKeys]]) {
    return primary;
  }
  NSMutableDictionary<NSString*, UIColor*>* resolved = [fallback mutableCopy];
  [resolved addEntriesFromDictionary:primary];
  return [resolved copy];
}

}  // namespace

@implementation BackendPromoCustomUIViewController {
  BackendPromoCustomUIParams* _params;
}

+ (UIViewController*)
    viewControllerWithParams:(BackendPromoCustomUIParams*)params
               actionHandler:(id<ConfirmationAlertActionHandler>)actionHandler {
  NSBundle* bundle = base::apple::FrameworkBundle();
  NSString* jsonPath = nil;
  if (params.imageURL.length > 0) {
    NSString* resourceName = [params.imageURL stringByDeletingPathExtension];
    jsonPath = [bundle pathForResource:resourceName ofType:@"json"];
  }

  if (jsonPath != nil) {
    BackendPromoCustomUIViewController* animatedViewController =
        [[BackendPromoCustomUIViewController alloc] initWithParams:params];
    animatedViewController.actionHandler = actionHandler;
    return animatedViewController;
  }

  ConfirmationAlertViewController* staticViewController =
      [[ConfirmationAlertViewController alloc] init];
  staticViewController.titleString = params.title;
  if (params.instructionSteps.count > 0) {
    staticViewController.underTitleView =
        [[InstructionView alloc] initWithList:params.instructionSteps];
    staticViewController.subtitleString = nil;
  } else {
    staticViewController.subtitleString = params.body;
  }
  staticViewController.configuration.primaryActionString =
      params.primaryActionTitle;
  staticViewController.configuration.secondaryActionString =
      params.secondaryActionTitle;
  [staticViewController reloadConfiguration];
  staticViewController.actionHandler = actionHandler;
  if (params.imageURL.length > 0) {
    staticViewController.image =
        [UIImage imageNamed:params.imageURL
                                 inBundle:base::apple::FrameworkBundle()
            compatibleWithTraitCollection:nil];
  }
  return staticViewController;
}

- (instancetype)initWithParams:(BackendPromoCustomUIParams*)params {
  self = [super init];
  if (self) {
    _params = params;
    self.titleString = params.title;
    if (params.instructionSteps.count > 0) {
      self.underTitleView =
          [[InstructionView alloc] initWithList:params.instructionSteps];
    } else {
      self.subtitleString = params.body;
    }
    self.primaryActionString = params.primaryActionTitle;
    self.secondaryActionString = params.secondaryActionTitle;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.useLegacyDarkMode = NO;
  self.animationName = [_params.imageURL stringByDeletingPathExtension];

  BackendPromoLottieParams* lottieParams = _params.lottieParams;
  NSDictionary<NSString*, NSString*>* lightColorMapping =
      lottieParams.lightColorMapping;
  NSDictionary<NSString*, NSString*>* darkColorMapping =
      lottieParams.darkColorMapping;

  NSDictionary<NSString*, UIColor*>* light_color_provider =
      ColorProviderFromMapping(lightColorMapping);
  NSDictionary<NSString*, UIColor*>* dark_color_provider =
      ColorProviderFromMapping(darkColorMapping);

  self.lightModeColorProvider =
      ResolveColorProvider(light_color_provider, dark_color_provider);
  self.darkModeColorProvider =
      ResolveColorProvider(dark_color_provider, light_color_provider);
  self.animationTextProvider = lottieParams.textMapping;

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kBackendPromoCustomUIViewAXID;
}

@end
