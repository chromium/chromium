// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_

#import <UIKit/UIKit.h>

#include <string>
#include <vector>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "url/gurl.h"

@class AutofillLegalMessageLine;
@class CrURL;

namespace autofill {

class EntityInstance;

// Encapsulates the complete footer UI state for an Autofill AI entity.
struct AutofillAiEntityFooterContent {
  // The full footer text with link tags.
  NSString* text = nil;
  // All link URLs appearing in `text`, in tag order, for UI consumption.
  NSArray<CrURL*>* urls = nil;
  // The subset of link URLs originating from legal disclosure messages, used
  // for funnel metrics logging.
  std::vector<GURL> legal_message_urls;
};

// Returns the default icon for the Autofill AI entity type.
UIImage* DefaultIconForAutofillAiEntityType(EntityTypeName entity_type_name,
                                            bool is_personal_context,
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

// Returns the complete footer content for `entity`: combines the storage notice
// and any formatted `legal_messages` separated by a blank line, and returns
// both the complete list of link `urls` and the subset of GURL belonging to
// `legal_messages`.
AutofillAiEntityFooterContent GetEntityFooterContent(
    const EntityInstance& entity,
    bool is_update,
    const std::u16string& user_email,
    NSArray<AutofillLegalMessageLine*>* legal_messages);

// Returns the URL for "manage your info" link for save to wallet footer.
GURL GetManageYourInfoURL();

// Returns the URL for the Google Privacy Policy link.
GURL GetGooglePrivacyPolicyURL();

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
int GetSaveToWalletSubtitleStringId(bool is_private_pass);

// Returns the message ID for the accept button in the save entity dialog.
// Exposed so the Consent Auditor records the exact string shown in the UI.
int GetSaveEntityAcceptButtonStringId();

// Returns whether two arrays of `CrURL` are equal by comparing their `GURL`
// values.
bool AreCrURLArraysEqual(NSArray<CrURL*>* array1, NSArray<CrURL*>* array2);

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_
