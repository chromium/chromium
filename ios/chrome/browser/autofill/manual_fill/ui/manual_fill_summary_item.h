// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_SUMMARY_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_SUMMARY_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_icon_configuration.h"

@class FaviconAttributes;

// UI item for a summary row in the expanded manual fill view.
@interface ManualFillSummaryItem : NSObject

// Stable identifier for the underlying manual fill entry.
@property(nonatomic, copy, readonly) NSString* identifier;

// Primary title of the row.
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle below `title`.
@property(nonatomic, copy, readonly) NSString* subtitle;

// Style for the leading icon.
@property(nonatomic, assign, readonly) ManualFillIconStyle iconStyle;

// Leading image (or optional symbol override for `kFavicon`).
@property(nonatomic, strong, readonly) UIImage* iconImage;

// Favicon attributes used when `iconStyle` is `kFavicon`.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

// Optional accessibility prefix for the cell's 1-based index (e.g., "1 of 3").
@property(nonatomic, copy) NSString* cellIndexAccessibilityLabel;

// Designated initializer. `identifier` and `title` must be non-empty.
- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
                          subtitle:(NSString*)subtitle
                         iconStyle:(ManualFillIconStyle)iconStyle
                         iconImage:(UIImage*)iconImage
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Configures `cell` (dequeued via `TableViewCellContentConfiguration`).
- (void)configureCell:(UITableViewCell*)cell;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_SUMMARY_ITEM_H_
