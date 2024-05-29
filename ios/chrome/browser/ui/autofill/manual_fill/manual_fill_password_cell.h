// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class FaviconAttributes;
@class FaviconView;
class GURL;
@protocol ManualFillContentInjector;
@class ManualFillCredential;

extern NSString* const kMaskedPasswordTitle;

// Wrapper to show password cells in a LegacyChromeTableViewController.
@interface ManualFillCredentialItem : TableViewItem

// URL to fetch the favicon.
@property(nonatomic, readonly) const GURL& faviconURL;

// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

// The cell won't show a title (site name) label if it is connected to the
// previous password item.
@property(nonatomic, readonly) BOOL isConnectedToPreviousItem;

// `cellIndexAccessibilityLabel` is the part of the cell's accessibility label
// that is used to indicate the index at which the password represented by this
// item is positioned in the list of passwords to show.
// TODO(crbug.com/326398845): Remove the `isConnectedToPreviousItem` and
// `isConnectedToNextItem` params once the Keyboard Accessory Upgrade feature
// has launched both on iPhone and iPad.
- (instancetype)initWithCredential:(ManualFillCredential*)credential
         isConnectedToPreviousItem:(BOOL)isConnectedToPreviousItem
             isConnectedToNextItem:(BOOL)isConnectedToNextItem
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                       menuActions:(NSArray<UIAction*>*)menuActions
       cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Cell to display a Credential where the username and password are interactable
// and send the data to the delegate.
@interface ManualFillPasswordCell : TableViewCell

// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

// Updates the cell with the `credential`. If the user iteracts with it, the
// `contentInjector` will be notified. `menuActions` are the UIActions that
// should be available from the cell's overflow menu button.
// `cellIndexAccessibilityLabel` is the part of this cell's accessibility label
// that is used to indicate the index at which the password represented by this
// cell is positioned in the list of passwords to show.
- (void)setUpWithCredential:(ManualFillCredential*)credential
      isConnectedToPreviousCell:(BOOL)isConnectedToPreviousCell
          isConnectedToNextCell:(BOOL)isConnectedToNextCell
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel;

// Configures the cell for the passed favicon attributes.
- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_
