// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Configures the title view of this ViewController.
UIView* SetUpTitleView() {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_TITLE);
  UIView* title_view = password_manager::CreatePasswordManagerTitleView(title);
  return title_view;
}

ButtonStackConfiguration* SetUpButtons() {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_CREATE);
  configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_SAVE_ANOTHER_WAY);
  return configuration;
}

}  // namespace

@interface PasskeyCreationBottomSheetViewController () {
  // The username for the passkey request.
  NSString* _username;

  // The email for the passkey request.
  NSString* _email;

  // URL of the current page the bottom sheet is being displayed on.
  GURL _url;

  // The passkey creation handler for user actions.
  __weak id<BrowserCoordinatorCommands> _handler;
}

@end

@implementation PasskeyCreationBottomSheetViewController

- (instancetype)initWithHandler:(id<BrowserCoordinatorCommands>)handler {
  ButtonStackConfiguration* configuration = SetUpButtons();
  self = [super initWithConfiguration:configuration];
  if (self) {
    _handler = handler;
  }
  return self;
}

- (void)viewDidLoad {
  self.view.accessibilityViewIsModal = YES;

  // Set the properties read by the base class when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.imageHasFixedSize = YES;
  self.showsVerticalScrollIndicator = NO;
  self.topAlignedLayout = YES;

  self.aboveTitleView = SetUpTitleView();

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_TITLE);
  self.titleTextStyle = UIFontTextStyleTitle2;

  self.secondaryTitleString = [self secondaryTitle];

  self.subtitleString = [self subtitle];
  self.subtitleTextStyle = UIFontTextStyleFootnote;

  [super viewDidLoad];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    // TODO(crbug.com/460485496): Dismiss the bottom sheet properly.
  }
}

#pragma mark - PasskeyCreationBottomSheetConsumer

- (void)setUsername:(NSString*)username email:(NSString*)email url:(GURL)url {
  _username = username;
  _email = email;
  _url = url;
}

#pragma mark - Private

// Configures the secondary title string of this ViewController.
- (NSString*)secondaryTitle {
  // TODO(crbug.com/460485496): Format this text in a UIView and set
  // `self.underTitleView` instead of `self.secondaryTitleString`.
  NSString* host = base::SysUTF8ToNSString(_url.host());
  NSString* username = _username;
  return [NSString stringWithFormat:@"%@\n%@", username ?: @"", host];
}

// Configures the subtitle string of this ViewController.
- (NSString*)subtitle {
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_SUBTITLE);
  NSString* accountInfo =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_ACCOUNT);
  NSString* email = _email;
  return
      [NSString stringWithFormat:@"%@\n%@\n%@", subtitle, accountInfo, email];
}

@end
