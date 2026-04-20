// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "url/gurl.h"

namespace autofill {

// Returns the default icon for the Autofill AI entity type.
UIImage* DefaultIconForAutofillAiEntityType(EntityTypeName entity_type_name,
                                            CGFloat symbol_point_size,
                                            UIColor* tint_color);

// Returns the display name for the given Autofill AI attribute type. For most
// attribute types, the display name is the same as the attribute type name.
// For attribute types with long names, this function returns a localized
// shorter name.
NSString* DisplayNameForAutofillAiAttributeType(AttributeType attribute_type);

// Returns the title for a dialog asking to save an entity.
NSString* GetDialogTitleForSaveEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog asking to update an entity.
NSString* GetDialogTitleForUpdateEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog asking to add an entity.
NSString* GetDialogTitleForAddEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog to view an entity.
NSString* GetDialogTitleForViewEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog to edit an entity.
NSString* GetDialogTitleForEditEntity(EntityTypeName entity_type_name);

// Returns the footer text for saving an entity to Wallet, formatted with the
// user's email.
NSString* GetSaveEntityToWalletFooterText(NSString* user_email);

// Returns the footer text for updating an entity saved in Wallet, formatted
// with the user's email.
NSString* GetUpdateEntitySavedInWalletFooterText(NSString* user_email);

// Returns the URL for "manage your info" link for save to wallet footer.
GURL GetManageYourInfoURL();

// Returns the URL for Google Wallet passes.
GURL GetGoogleWalletPassesURL();

// Returns the logo for Google Wallet. When branded assets are not available,
// a placeholder symbol is returned instead.
UIImage* GetWalletLogo(CGFloat point_size, UIColor* tint_color);

// Returns a view for use as the title on surfaces where the user is about to
// save (update, etc) an entity in Google Wallet. Should not be used for local
// saves. `title` is the desired text to display (e.g., "Add Passport").
UIView* CreateBrandedTitleForWalletSave(NSString* title);

// Returns the message ID for the subtitle shown in the save-to-wallet dialog.
// Exposed so the Consent Auditor records the exact string shown in the UI.
// Note: The string contains placeholders for the Google Wallet title, a
// link, the Google Wallet title again, and the user's email.
int GetSaveToWalletSubtitleStringId();

// Returns the message ID for the accept button in the save entity dialog.
// Exposed so the Consent Auditor records the exact string shown in the UI.
int GetSaveEntityAcceptButtonStringId();

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_
