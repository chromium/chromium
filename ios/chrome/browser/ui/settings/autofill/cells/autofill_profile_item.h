// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_

#include <string>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_address_profile_record_type.h"

// Item for autofill profile (address).
@interface AutofillProfileItem : TableViewItem

// The image in the cell. If nil, won't be added to the view hierarchy.
@property(nonatomic, readwrite, strong) UIImage* image;

// The text label in the cell.
@property(nonatomic, readwrite, copy) NSString* title;

// Detail text to be displayed. The detail text label is configured with
// multiline (no limit).
@property(nonatomic, strong) NSString* detailText;

// The GUID used by the PersonalDataManager to identify profiles.
@property(nonatomic, assign) std::string GUID;

// Denotes whether the profile is local, syncable or account profile.
@property(nonatomic, assign)
    AutofillAddressProfileRecordType autofillProfileRecordType;

// If YES, a section is shown to the user containing a button to migrate the
// profile to Account.
@property(nonatomic, assign) BOOL showMigrateToAccountButton;

// YES, if the cloud off icon representing local profile is shown.
@property(nonatomic, assign) BOOL localProfileIconShown;

@end

@interface AutofillProfileCell : TableViewCell

// The cell imageView.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// The cell text.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// The cell detail text.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;
// YES, if the cloud off icon representing local profile is shown.
@property(nonatomic, assign) BOOL localProfileIconShown;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_
