// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ACTION_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ACTION_H_

#import <UIKit/UIKit.h>

// UI item for a footer action row in the expanded manual fill view.
@interface ManualFillAction : NSObject

// Stable identifier for the action.
@property(nonatomic, copy, readonly) NSString* identifier;

// Localized title displayed in the action row.
@property(nonatomic, copy, readonly) NSString* title;

// Optional accessibility identifier applied to the configured cell.
@property(nonatomic, copy, readonly) NSString* accessibilityIdentifier;

// Designated initializer. `identifier` and `title` must be non-empty.
- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
           accessibilityIdentifier:(NSString*)accessibilityIdentifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Configures `cell` (dequeued via `TableViewCellContentConfiguration`).
- (void)configureCell:(UITableViewCell*)cell;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ACTION_H_
