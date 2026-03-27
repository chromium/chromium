// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/age_mismatch_signout_view_controller.h"

#import "ios/chrome/browser/authentication/ui_bundled/views/identity_view.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr CGFloat kIdentityViewCornerRadius = 8.0;

}  // namespace

@interface AgeMismatchSignoutViewController ()
// View displaying the identity that was signed out.
@property(nonatomic, strong) IdentityView* identityView;
@end

@implementation AgeMismatchSignoutViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kImage;
  self.headerBackgroundImage =
      [UIImage imageNamed:@"age_mismatch_prompt_image"];
  self.modalInPresentation = YES;

  BOOL isIPad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  self.titleText =
      l10n_util::GetNSString(isIPad ? IDS_IOS_AGE_MISMATCH_HEADER_IPAD
                                    : IDS_IOS_AGE_MISMATCH_HEADER_IPHONE);

  self.subtitleText =
      l10n_util::GetNSString(isIPad ? IDS_IOS_AGE_MISMATCH_SUBTITLE_IPAD
                                    : IDS_IOS_AGE_MISMATCH_SUBTITLE_IPHONE);

  self.disclaimerText = l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_DISCLAIMER);

  // TODO(crbug.com/483935544): Update the disclaimer URL.
  self.disclaimerURLs = @[ [[NSURL alloc] initWithString:@""] ];

  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_PRIMARY_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_SECONDARY_BUTTON);

  // Add the identity view.
  [self.specificContentView addSubview:self.identityView];

  [NSLayoutConstraint activateConstraints:@[
    [self.identityView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [self.identityView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.identityView.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],
    [self.identityView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - Properties

- (IdentityView*)identityView {
  if (!_identityView) {
    _identityView = [[IdentityView alloc] initWithFrame:CGRectZero];
    _identityView.translatesAutoresizingMaskIntoConstraints = NO;
    _identityView.layer.cornerRadius = kIdentityViewCornerRadius;
    _identityView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];

    // IdentityView defines its vertical bounds using inequality constraints
    // (greaterThanOrEqualTo) so it can be optionally stretched and centered.
    // Because it lacks an equality constraint or an intrinsic content size,
    // it will stretch uncontrollably unless countered. We use a low-priority
    // height constraint to pull it tightly around its contents.
    NSLayoutConstraint* heightConstraint =
        [_identityView.heightAnchor constraintEqualToConstant:0];
    heightConstraint.priority = UILayoutPriorityDefaultLow - 1;
    heightConstraint.active = YES;
  }
  return _identityView;
}

#pragma mark - AgeMismatchSignoutConsumer

- (void)setPrimaryIdentityName:(NSString*)name
                         email:(NSString*)email
                        avatar:(UIImage*)avatar
                       managed:(BOOL)managed {
  [self.identityView setTitle:name ? name : email
                     subtitle:name ? email : nil
                      managed:managed];
  [self.identityView setAvatar:avatar];
}

@end
