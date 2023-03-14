// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_TOOLS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_TOOLS_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"

// Item for a tools menu item.
@interface PopupMenuToolsItem : TableViewItem<PopupMenuItem>

// The title of the item.
@property(nonatomic, copy) NSString* title;
// Image to be displayed on the item.
@property(nonatomic, strong) UIImage* image;
// Whether the cell associated with this item should be enabled.
@property(nonatomic, assign) BOOL enabled;
// Number to be displayed in the badge. If 0, the badge is hidden. This is not
// read by VoiceOver.
@property(nonatomic, assign) NSInteger badgeNumber;
// Text to be displayed in the badge. Set to nil to hide the badge. The text
// badge is only displayed if the numbered badge is hidden. This is not read by
// VoiceOver.
@property(nonatomic, copy) NSString* badgeText;
// Whether the item is associated with a destructive action. If `YES`, then a
// specific styling is applied.
@property(nonatomic, assign) BOOL destructiveAction;
// Additional label. Read after `title` if not nil.
@property(nonatomic, strong) NSString* additionalAccessibilityLabel;

@end

// Associated cell for the PopupMenuToolsItem.
@interface PopupMenuToolsCell : TableViewCell

// Image view to display the image.
@property(nonatomic, strong, readonly) UIImageView* imageView;

// Title label for the cell.
@property(nonatomic, strong, readonly) UILabel* titleLabel;

// Whether the cell is associated with a destructive action. If `YES`, then a
// specific styling is applied.
@property(nonatomic, assign) BOOL destructiveAction;

// Additional label. Read after `title` if not nil.
@property(nonatomic, strong) NSString* additionalAccessibilityLabel;

// Sets the number on the badge number.
- (void)setBadgeNumber:(NSInteger)badgeNumber;
// Sets the text of the badge text. Hides the badge text if `badgeText` is nil.
- (void)setBadgeText:(NSString*)badgeText;

// After this is called, the cell is listening for the
// UIContentSizeCategoryDidChangeNotification notification and updates its font
// size to the new category.
- (void)registerForContentSizeUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_CELLS_POPUP_MENU_TOOLS_ITEM_H_
