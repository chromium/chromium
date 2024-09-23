// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

NSString* const kPasswordsTableViewID = @"PasswordsTableViewID";
NSString* const kPasswordsSearchBarID = @"PasswordsSearchBar";
NSString* const kPasswordsScrimViewID = @"PasswordsScrimViewID";

NSString* const kPasswordDetailsTableViewID = @"PasswordDetailsTableViewID";
NSString* const kPasswordDetailsDeletionAlertViewID =
    @"PasswordDetailsDeletionAlertViewID";
NSString* const kPasswordsAddPasswordSaveButtonID =
    @"PasswordsAddPasswordSaveButtonID";
NSString* const kPasswordsAddPasswordCancelButtonID =
    @"PasswordsAddPasswordCancelButtonID";

NSString* const kAddPasswordButtonID = @"addPasswordItem";

NSString* const kPasswordIssuesTableViewID = @"kPasswordIssuesTableViewID";

NSString* const kDismissedWarningsCellID = @"DismissedWarningsCellID";

NSString* const kUsernameTextfieldForPasswordDetailsID =
    @"kUsernameTextfieldForPasswordDetailsID";

NSString* const kUserDisplayNameTextfieldForPasswordDetailsID =
    @"kUserDisplayNameTextfieldForPasswordDetailsID";

NSString* const kCreationDateTextfieldForPasswordDetailsID =
    @"kCreationDateTextfieldForPasswordDetailsID";

NSString* const kPasswordTextfieldForPasswordDetailsID =
    @"kPasswordTextfieldForPasswordDetailsID";

NSString* const kDeleteButtonForPasswordDetailsID =
    @"kDeleteButtonForPasswordDetailsID";

NSString* const kLocalOnlyPasswordIconID = @"kLocalOnlyPasswordIconID";

NSString* WidgetPromoImageName() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return kGooglePasswordManagerWidgetPromoImage;
#else
  return kChromiumPasswordManagerWidgetPromoImage;
#endif
}

NSString* WidgetPromoDisabledImageName() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return kGooglePasswordManagerWidgetPromoDisabledImage;
#else
  return kChromiumPasswordManagerWidgetPromoDisabledImage;
#endif
}

NSString* const kWidgetPromoID = @"WidgetPromoID";

NSString* const kWidgetPromoCloseButtonID = @"WidgetPromoCloseButtonID";

NSString* const kWidgetPromoImageID = @"WidgetPromoImageID";

const char kPasswordManagerWidgetPromoActionHistogram[] =
    "IOS.PasswordManager.WidgetPromo.Action";
