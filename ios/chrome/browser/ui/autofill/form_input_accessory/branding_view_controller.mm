// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/branding_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/autofill/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The left margin of the branding logo, if visible.
constexpr CGFloat kBrandingLeadingInset = 10;
}  // namespace

@interface BrandingViewController ()

// Delegate to handle interactions.
@property(nonatomic, readonly, weak) id<BrandingViewControllerDelegate>
    delegate;

@end

@implementation BrandingViewController

- (instancetype)initWithDelegate:(id<BrandingViewControllerDelegate>)delegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)loadView {
  NSString* logoName;
  switch (autofill::features::GetAutofillBrandingType()) {
    case autofill::features::AutofillBrandingType::kFullColor:
      logoName = @"fullcolor_branding_icon";
      break;
    case autofill::features::AutofillBrandingType::kMonotone:
      logoName = @"monotone_branding_icon";
      break;
    case autofill::features::AutofillBrandingType::kDisabled:
      NOTREACHED();
      break;
  }
  UIImage* logo = [[UIImage imageNamed:logoName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  if (@available(iOS 15.0, *)) {
    UIButtonConfiguration* buttonConfig =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfig.contentInsets =
        NSDirectionalEdgeInsetsMake(0, kBrandingLeadingInset, 0, 0);
    button.configuration = buttonConfig;
  } else {
    button.imageEdgeInsets = UIEdgeInsetsMake(0, kBrandingLeadingInset, 0, 0);
  }
  [button setImage:logo forState:UIControlStateNormal];
  [button setImage:logo forState:UIControlStateHighlighted];
  [button addTarget:self.delegate
                action:@selector(brandingIconPressed)
      forControlEvents:UIControlEventTouchUpInside];
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
  button.isAccessibilityElement = NO;  // Prevents VoiceOver users from tap.
  button.translatesAutoresizingMaskIntoConstraints = NO;
  self.view = button;
}

- (void)viewDidAppear:(BOOL)animated {
  // TODO(crbug.com/1358671): perform animation.
}

@end
