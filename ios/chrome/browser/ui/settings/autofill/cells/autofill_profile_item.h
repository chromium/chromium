// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_

#include <string>

#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_address_profile_source.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"

// Item for autofill profile (address).
@interface AutofillProfileItem : TableViewMultiDetailTextItem

// The GUID used by the PersonalDataManager to identify profiles.
@property(nonatomic, assign) std::string GUID;

// Denotes whether the profile is local, syncable or account profile.
@property(nonatomic, assign) AutofillAddressProfileSource autofillProfileSource;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_
