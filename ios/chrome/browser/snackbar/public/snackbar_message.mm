// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/public/snackbar_message.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/browser/snackbar/public/snackbar_message_action.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size for the icons.
constexpr CGFloat kSymbolsPointSize = 24;

// Default snackbar visibility duration.
const NSTimeInterval kDefaultDuration = 4;

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolsPointSize));
#else
  return MakeSymbolMulticolor(
      DefaultSymbolWithPointSize(@"gearshape.2", kSymbolsPointSize));
#endif
}

// Returns a tinted version of the enterprise building icon.
UIImage* GetEnterpriseIcon() {
  UIColor* color = [UIColor colorNamed:kTextSecondaryColor];
  return SymbolWithPalette(
      CustomSymbolWithPointSize(kEnterpriseSymbol, kSymbolsPointSize),
      @[ color ]);
}

bool CanShowManagementMessaging(const ManagementState& management_state) {
  return management_state.is_profile_managed() ||
         (AreSeparateProfilesForManagedAccountsEnabled() &&
          management_state.is_managed());
}

}  // namespace

@implementation SnackbarMessage

- (instancetype)initWithTitle:(NSString*)title {
  self = [super init];
  if (self) {
    _title = [title copy];
    _duration = kDefaultDuration;
  }
  return self;
}

- (instancetype)initWithMDCSnackbarMessage:(MDCSnackbarMessage*)message {
  NSString* title;
  if ([message isKindOfClass:[IdentitySnackbarMessage class]]) {
    IdentitySnackbarMessage* identityMessage =
        (IdentitySnackbarMessage*)message;
    title =
        (identityMessage.name)
            ? l10n_util::GetNSStringF(
                  IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                  base::SysNSStringToUTF16(identityMessage.name))
            : l10n_util::GetNSString(IDS_IOS_SIGNIN_ACCOUNT_NOTIFICATION_TITLE);
  } else {
    title = message.text;
  }

  self = [self initWithTitle:title];
  if (self) {
    if ([message isKindOfClass:[IdentitySnackbarMessage class]]) {
      IdentitySnackbarMessage* identityMessage =
          (IdentitySnackbarMessage*)message;
      if (CanShowManagementMessaging(identityMessage.managementState)) {
        self.subtitle = identityMessage.email;
        if (AreSeparateProfilesForManagedAccountsEnabled()) {
          self.secondarySubtitle = l10n_util::GetNSString(
              identityMessage.managementState.is_browser_managed()
                  ? IDS_IOS_ENTERPRISE_BROWSER_MANAGED
                  : IDS_IOS_ENTERPRISE_ACCOUNT_MANAGED);
        } else {
          self.secondarySubtitle = l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_MANAGED_BY_YOUR_ORGANIZATION);
        }
      } else {
        self.subtitle = identityMessage.email;
      }
      self.leadingAccessoryImage = identityMessage.avatar;
      self.roundLeadingAccessoryView = YES;

      BOOL showManagementMessaging =
          CanShowManagementMessaging(identityMessage.managementState);
      UIImage* accountBadge = showManagementMessaging
                                  ? GetEnterpriseIcon()
                                  : GetBrandedGoogleServicesSymbol();
      self.trailingAccessoryImage = accountBadge;
      self.roundTrailingAccessoryView = YES;
    }

    self.duration = message.duration;
    self.completionHandler = message.completionHandler;
    if (message.action) {
      _action = [[SnackbarMessageAction alloc] init];
      _action.title = message.action.title;
      _action.handler = message.action.handler;
      _action.accessibilityLabel = message.action.accessibilityLabel;
      _action.accessibilityHint = message.action.accessibilityHint;
    }
  }
  return self;
}

@end
