// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_

#include <string>

#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_address_profile_record_type.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"

// Item for autofill profile (address).
@interface AutofillProfileItem : TableViewItem

// The image in the cell. If nil, won't be added to the view hierarchy.
@property(nonatomic, readwrite, strong) UIImage* image;

// The text label in the cell.
@property(nonatomic, readwrite, copy) NSString* title;

// Detail text to be displayed. The detail text label is configured with
// multiline (no limit).
@property(nonatomic, copy) NSString* detailText;

// Trailing detail text to be displayed.
@property(nonatomic, copy) NSString* trailingDetailText;

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

@interface AutofillProfileCell : LegacyTableViewCell

// The cell imageView.
@property(nonatomic, readonly, strong) UIImageView* imageView;

// YES, if the cloud off icon representing local profile is shown.
@property(nonatomic, assign) BOOL localProfileIconShown;

// Sets the textLabel.
- (void)setText:(NSString*)text;

// Sets the detailTextLabel.
- (void)setDetailText:(NSString*)detailText;

// Sets the visibility of trailingDetailTextLabel.
- (void)setTrailingDetailText:(NSString*)trailingText;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_CELLS_AUTOFILL_PROFILE_ITEM_H_
