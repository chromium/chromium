// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_

#import <UIKit/UIKit.h>

#ifdef __cplusplus
#import <set>
#endif

#import "base/ios/block_types.h"

#import "ios/chrome/browser/ui/menu/menu_histograms.h"

@class CrURL;
@protocol SystemIdentity;
#ifdef __cplusplus
class TabGroup;
#endif

// Factory providing methods to create UIActions with consistent titles, images
// and metrics structure. When using any action from this class, an histogram
// will be recorded on Mobile.ContextMenu.<Scenario>.Action.
@interface ActionFactory : NSObject

// Initializes a factory instance to create action instances for the given
// `scenario`. `scenario` is used to choose the histogram in which to record the
// actions.
- (instancetype)initWithScenario:(enum MenuScenarioHistogram)scenario;

// Creates a UIAction instance configured to copy the given `URL` to the
// pasteboard.
- (UIAction*)actionToCopyURL:(CrURL*)URL;

// Creates a UIAction instance configured for sharing which will invoke
// the given `block` upon execution.
- (UIAction*)actionToShareWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured to show the full `URLString` that
// appears in the web context menu and which will invoke the given `block` upon
// execution.
- (UIAction*)actionToShowFullURL:(NSString*)URLString
                           block:(ProceduralBlock)block;

// Creates a UIAction instance configured for pinning a tab which will invoke
// the given `block` upon execution.
- (UIAction*)actionToPinTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for unpinning a tab which will invoke
// the given `block` upon execution.
- (UIAction*)actionToUnpinTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for deletion which will invoke
// the given delete `block` when executed.
- (UIAction*)actionToDeleteWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for opening a
// URL in a new tab. When triggered, the action will invoke the `block` which
// needs to open a URL in a new tab.
- (UIAction*)actionToOpenInNewTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for opening
// multiple URLs in new tabs. When triggered, the action will invoke the `block`
// which needs to open URLs in new tabs.
- (UIAction*)actionToOpenAllTabsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for suppression which will invoke
// the given delete `block` when executed.
- (UIAction*)actionToRemoveWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for editing
// which will invoke the given edit `block` when executed.
- (UIAction*)actionToEditWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for hiding which will invoke
// the given hiding `block` when executed.
- (UIAction*)actionToHideWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for moving a folder which will invoke
// the given `block` when executed.
- (UIAction*)actionToMoveFolderWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for marking an entry from the
// ReadingList as read, which will invoke the given `block` when executed.
- (UIAction*)actionToMarkAsReadWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for marking an entry from the
// ReadingList as unread, which will invoke the given `block` when executed.
- (UIAction*)actionToMarkAsUnreadWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for viewing
// an offline version of an URL in a new tab. When triggered, the action will
// invoke the `block` when executed.
- (UIAction*)actionToOpenOfflineVersionInNewTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for adding to the reading list.
- (UIAction*)actionToAddToReadingListWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for adding to bookmarks.
- (UIAction*)actionToBookmarkWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for editing a bookmark.
- (UIAction*)actionToEditBookmarkWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for closing a regular tab.
- (UIAction*)actionToCloseRegularTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for closing a pinned tab.
- (UIAction*)actionToClosePinnedTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for closing all the other tabs.
- (UIAction*)actionToCloseAllOtherTabsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for saving an image.
- (UIAction*)actionSaveImageWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for copying an image.
- (UIAction*)actionCopyImageWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for searching an image with given search service
// `title`. Invokes the given `completion` block after execution.
- (UIAction*)actionSearchImageWithTitle:(NSString*)title
                                  Block:(ProceduralBlock)block;

// Creates a UIAction instance for closing all tabs.
- (UIAction*)actionToCloseAllTabsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for entering tab selection mode.
- (UIAction*)actionToSelectTabsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for searching an image with Lens.
// Invokes the given `completion` block after execution.
- (UIAction*)actionToSearchImageUsingLensWithBlock:(ProceduralBlock)block;

// Updates the given `ProceduralBlock` to record the
// `MobileWebContextMenuOpenTab` user action.
- (ProceduralBlock)recordMobileWebContextMenuOpenTabActionWithBlock:
    (ProceduralBlock)block;

// Creates a UIAction instance for adding `tabsNumber` tab in a new tab group.
// `inSubmenu` changes the string to be displayed.
- (UIAction*)actionToAddTabsToNewGroupWithTabsNumber:(int)tabsNumber
                                           inSubmenu:(BOOL)inSubmenu
                                               block:(ProceduralBlock)block;

// Creates a UIAction instance for opening  a link in new tab group. `inSubmenu`
// changes the string to be displayed.
- (UIAction*)actionToOpenLinkInNewGroupWithBlock:(ProceduralBlock)block
                                       inSubmenu:(BOOL)inSubmenu;

#ifdef __cplusplus
// Creates a UIMenu instance for adding a tab to an existing group or to a new
// group using a block that takes a group as an argument. This argument will be
// `nullptr` if it should be added to a new group.
//
// If there is no existing groups, it will only have the option to add to a new
// group.
- (UIMenuElement*)
    menuToAddTabToGroupWithGroups:(const std::set<const TabGroup*>&)groups
                     numberOfTabs:(int)tabsNumber
                            block:(void (^)(const TabGroup*))block;

// Creates a UIMenu instance for opening a link in an existing group or in a new
// group using a block that takes a group as an argument. This argument will be
// `nullptr` if it should be added to a new group. If there is no existing
// groups, it will only have the option to open in a new group.
- (UIMenuElement*)
    menuToOpenLinkInGroupWithGroups:(const std::set<const TabGroup*>&)groups
                              block:(void (^)(const TabGroup*))block;

// Creates a UIMenu instance for moving a tab from `currentGroup` in or out of
// an existing group. `groups` cannot be empty.
- (UIMenuElement*)
    menuToMoveTabToGroupWithGroups:(const std::set<const TabGroup*>&)groups
                      currentGroup:(const TabGroup*)currentGroup
                         moveBlock:(void (^)(const TabGroup*))moveBlock
                       removeBlock:(ProceduralBlock)removeBlock;
#endif

// Creates a UIAction instance for renaming a tab group.
- (UIAction*)actionToRenameTabGroupWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for adding a new tab to the tab group.
- (UIAction*)actionToAddNewTabInGroupWithBlock:(ProceduralBlock)block
    NS_SWIFT_NAME(actionToAddNewTabInGroup(with:));

// Creates a UIAction instance for ungrouping a tab group.
- (UIAction*)actionToUngroupTabGroupWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for deleting a tab group.
- (UIAction*)actionToDeleteTabGroupWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for closing a tab group.
- (UIAction*)actionToCloseTabGroupWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for showing
// details, which will invoke the given `block` when executed.
- (UIAction*)actionToShowDetailsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance to sort drive items by name.
- (UIAction*)actionToSortDriveItemsByNameWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance to sort drive items by modification time.
- (UIAction*)actionToSortDriveItemsByModificationTimeWithBlock:
    (ProceduralBlock)block;

// Creates a UIAction instance to sort drive items by opening time.
- (UIAction*)actionToSortDriveItemsByOpeningTimeWithBlock:
    (ProceduralBlock)block;

// Creates a UIMenu instance for identity selection within drive file picker.
- (UIMenuElement*)
    menuToSelectDriveIdentityWithIdentities:
        (NSArray<id<SystemIdentity>>*)identities
                            currentIdentity:(id<SystemIdentity>)currentIdentity
                                      block:(void (^)(const id<SystemIdentity>))
                                                block;

// Creates a UIAction instance to add an account to choose drive files from.
- (UIAction*)actionToAddAccountForDriveWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for showing
// manage in a new tab, which will invoke the given `block` when executed.
- (UIAction*)actionToManageLinkInNewTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance to show the recent activity in a shared tab
// group.
- (UIAction*)actionToShowRecentActivity:(ProceduralBlock)block;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_
