// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/multi_profile_passkey_creation_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details.h"

namespace {

// Returns the background color for this view.
UIColor* GetBackgroundColor() {
  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

}  // namespace

@interface MultiProfilePasskeyCreationViewController () <
    PromoStyleViewControllerDelegate>

@end

@implementation MultiProfilePasskeyCreationViewController {
  // The view to be used as the navigation bar title view.
  UIView* _navigationItemTitleView;

  // Email address associated with the signed in account.
  NSString* _userEmail;

  // Information about a passkey credential request.
  PasskeyRequestDetails* _passkeyRequestDetails;

  // The gaia ID associated with the current account
  NSString* _gaia;

  // Delegate for this view controller.
  __weak id<MultiProfilePasskeyCreationViewControllerDelegate>
      _multiProfilePasskeyCreationViewControllerDelegate;
}

- (instancetype)
            initWithDetails:(PasskeyRequestDetails*)passkeyRequestDetails
                       gaia:(NSString*)gaia
                  userEmail:(NSString*)userEmail
    navigationItemTitleView:(UIView*)navigationItemTitleView
                   delegate:
                       (id<MultiProfilePasskeyCreationViewControllerDelegate>)
                           delegate {
  self = [super initWithTaskRunner:nullptr];
  if (self) {
    _userEmail = userEmail;
    _passkeyRequestDetails = passkeyRequestDetails;
    _gaia = gaia;
    _multiProfilePasskeyCreationViewControllerDelegate = delegate;
    _navigationItemTitleView = navigationItemTitleView;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerName = @"passkey_generic_banner";
  self.bannerSize = BannerImageSizeType::kExtraShort;

  self.primaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_CREATE", @"Create");
  self.secondaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_CANCEL", @"Cancel");

  [super viewDidLoad];

  self.view.backgroundColor = GetBackgroundColor();
  self.navigationItem.titleView = _navigationItemTitleView;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_multiProfilePasskeyCreationViewControllerDelegate
      validateUserAndCreatePasskeyWithDetails:_passkeyRequestDetails
                                         gaia:_gaia];
}

- (void)didTapSecondaryActionButton {
  [_multiProfilePasskeyCreationViewControllerDelegate
      multiProfilePasskeyCreationViewControllerShouldBeDismissed:self];
}
@end
