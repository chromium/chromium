// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class FaviconAttributes;
@class FaviconView;
class GURL;
@protocol ManualFillContentInjector;
@class ManualFillCredential;

// Wrapper to show password cells in a LegacyChromeTableViewController.
@interface ManualFillCredentialItem : TableViewItem

// URL to fetch the favicon.
@property(nonatomic, readonly) const GURL& faviconURL;

// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

// Username associated with this item.
@property(nonatomic, readonly) NSString* username;

// The cell won't show a title (site name) label if it is connected to the
// previous password item.
@property(nonatomic, readonly) BOOL isConnectedToPreviousItem;

// `cellIndex` indicates the index (0-based) at which the password represented
// by this item is positioned in the list of passwords to show.
// `cellIndexAccessibilityLabel` is the cell's accessibility label and is used
// to indicate the cell's index (1-based) and the number of available passwords
// to accessibility users.
// TODO(crbug.com/326398845): Remove the `isConnectedToPreviousItem` and
// `isConnectedToNextItem` params once the Keyboard Accessory Upgrade feature
// has launched both on iPhone and iPad.
- (instancetype)initWithCredential:(ManualFillCredential*)credential
         isConnectedToPreviousItem:(BOOL)isConnectedToPreviousItem
             isConnectedToNextItem:(BOOL)isConnectedToNextItem
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                       menuActions:(NSArray<UIAction*>*)menuActions
                         cellIndex:(NSInteger)cellIndex
       cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
            showAutofillFormButton:(BOOL)showAutofillFormButton
           fromAllPasswordsContext:(BOOL)fromAllPasswordsContext
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
// should be available from the cell's overflow menu button. `cellIndex`
// indicates the index (0-based) at which the password represented by this cell
// is positioned in the list of passwords to show. `cellIndexAccessibilityLabel`
// is the cell's accessibility label and is used to indicate the cell's index
// (1-based) and the number of available passwords to accessibility users.
// `fromAllPasswordsContext` indicates whether the cell is presented in the all
// password list.
- (void)setUpWithCredential:(ManualFillCredential*)credential
      isConnectedToPreviousCell:(BOOL)isConnectedToPreviousCell
          isConnectedToNextCell:(BOOL)isConnectedToNextCell
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton
        fromAllPasswordsContext:(BOOL)fromAllPasswordsContext;

// Configures the cell for the passed favicon attributes.
- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_
