// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_view_controller.h"

#import "build/branding_buildflags.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Height of the Google Pay logo used as the image above the title of the
// bottomsheet.
CGFloat const kGooglePayLogoHeight = 32;
#endif

}  // namespace

@interface SaveCardBottomSheetViewController () <ConfirmationAlertActionHandler>
@end

// TODO(crbug.com/391366699): Implement SaveCardBottomSheetViewController.
@implementation SaveCardBottomSheetViewController {
  // Image to be displayed above the title of the bottomsheet.
  UIImage* _aboveTitleImage;
}

#pragma mark - SaveCardBottomSheetConsumer

- (void)setAboveTitleImage:(UIImage*)logoImage {
  _aboveTitleImage = logoImage;
}

- (void)setAboveTitleImageDescription:(NSString*)description {
  self.imageViewAccessibilityLabel = description;
}

- (void)setTitle:(NSString*)title {
  self.titleString = title;
}

- (void)setSubtitle:(NSString*)subtitle {
  self.subtitleString = subtitle;
}

- (void)setAcceptActionText:(NSString*)acceptActionText {
  self.primaryActionString = acceptActionText;
}

- (void)setCancelActionText:(NSString*)cancelActionText {
  self.secondaryActionString = cancelActionText;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [self aboveTitleImage];
  self.actionHandler = self;
  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertActionHandler

// Accept button was pressed.
- (void)confirmationAlertPrimaryAction {
  [self.mutator didAccept];
}

// Cancel button was pressed.
- (void)confirmationAlertSecondaryAction {
  [self.mutator didCancel];
}

#pragma mark - Private

// Returns the image to be used above the title of the bottomsheet.
- (UIImage*)aboveTitleImage {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // iOS-specific symbol is used to get an optimized image with better
  // resolution.
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGooglePaySymbol, kGooglePayLogoHeight));
#else
  return _aboveTitleImage;
#endif
}

@end
