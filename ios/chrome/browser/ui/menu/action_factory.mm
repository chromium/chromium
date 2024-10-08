// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/context_menu/context_menu_api.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface ActionFactory ()

// Histogram to record executed actions.
@property(nonatomic, assign) const char* histogram;

@end

@implementation ActionFactory

- (instancetype)initWithScenario:(MenuScenarioHistogram)scenario {
  if ((self = [super init])) {
    _histogram = GetActionsHistogramName(scenario);
  }
  return self;
}

- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                        type:(MenuActionType)type
                       block:(ProceduralBlock)block {
  // Capture only the histogram name's pointer to be copied by the block.
  const char* histogram = self.histogram;
  return [UIAction actionWithTitle:title
                             image:image
                        identifier:nil
                           handler:^(UIAction* action) {
                             base::UmaHistogramEnumeration(histogram, type);
                             if (block) {
                               block();
                             }
                           }];
}

- (UIAction*)actionToCopyURL:(CrURL*)URL {
  UIImage* image =
      DefaultSymbolWithPointSize(kLinkActionSymbol, kSymbolActionPointSize);
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE)
                image:image
                 type:MenuActionType::CopyURL
                block:^{
                  StoreURLInPasteboard(URL.gurl);
                }];
}

- (UIAction*)actionToShowFullURL:(NSString*)URLString
                           block:(ProceduralBlock)block {
  UIAction* action = [self actionWithTitle:nil
                                     image:nil
                                      type:MenuActionType::ShowFullURL
                                     block:block];
  action.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_SHOW_FULL_URL_BUTTON_ACCESSIBILITY_LABEL);
  action.attributes = UIMenuElementAttributesKeepsMenuPresented;
  action.subtitle = ios::provider::StyledContextMenuStringForString(URLString);
  return action;
}

- (UIAction*)actionToShareWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kShareSymbol, kSymbolActionPointSize);
  return
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL)
                      image:image
                       type:MenuActionType::Share
                      block:block];
}

- (UIAction*)actionToPinTabWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kPinSymbol, kSymbolActionPointSize);
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_PINTAB)
                image:image
                 type:MenuActionType::PinTab
                block:block];
}

- (UIAction*)actionToUnpinTabWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kPinSlashSymbol, kSymbolActionPointSize);
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_UNPINTAB)
                image:image
                 type:MenuActionType::UnpinTab
                block:block];
}

- (UIAction*)actionToDeleteWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kDeleteActionSymbol, kSymbolActionPointSize);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                      image:image
                       type:MenuActionType::Delete
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToOpenInNewTabWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kNewTabActionSymbol, kSymbolActionPointSize);
  ProceduralBlock completionBlock =
      [self recordMobileWebContextMenuOpenTabActionWithBlock:block];

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                         image:image
                          type:MenuActionType::OpenInNewTab
                         block:completionBlock];
}

- (UIAction*)actionToOpenAllTabsWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPEN_ALL_LINKS)
                         image:DefaultSymbolWithPointSize(
                                   kPlusSymbol, kSymbolActionPointSize)
                          type:MenuActionType::OpenAllInNewTabs
                         block:block];
}

- (UIAction*)actionToRemoveWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolActionPointSize);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_REMOVE_ACTION_TITLE)
                      image:image
                       type:MenuActionType::Remove
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToEditWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kEditActionSymbol, kSymbolActionPointSize);
  return [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE)
                         image:image
                          type:MenuActionType::Edit
                         block:block];
}

- (UIAction*)actionToHideWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolActionPointSize);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_HIDE_MENU_OPTION)
                      image:image
                       type:MenuActionType::Hide
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToMoveFolderWithBlock:(ProceduralBlock)block {
  // Use multi color to make sure the arrow is visible.
  UIImage* image = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMoveFolderSymbol, kSymbolActionPointSize));
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                image:image
                 type:MenuActionType::Move
                block:block];
}

- (UIAction*)actionToMarkAsReadWithBlock:(ProceduralBlock)block {
  UIImage* image = DefaultSymbolWithPointSize(kMarkAsReadActionSymbol,
                                              kSymbolActionPointSize);
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_READ_ACTION)
                         image:image
                          type:MenuActionType::Read
                         block:block];
}

- (UIAction*)actionToMarkAsUnreadWithBlock:(ProceduralBlock)block {
  UIImage* image = DefaultSymbolWithPointSize(kMarkAsUnreadActionSymbol,
                                              kSymbolActionPointSize);
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION)
                         image:image
                          type:MenuActionType::Unread
                         block:block];
}

- (UIAction*)actionToOpenOfflineVersionInNewTabWithBlock:
    (ProceduralBlock)block {
  UIImage* image = DefaultSymbolWithPointSize(kCheckmarkCircleSymbol,
                                              kSymbolActionPointSize);
  ProceduralBlock completionBlock =
      [self recordMobileWebContextMenuOpenTabActionWithBlock:block];

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON)
                         image:image
                          type:MenuActionType::ViewOffline
                         block:completionBlock];
}

- (UIAction*)actionToAddToReadingListWithBlock:(ProceduralBlock)block {
  UIImage* image = DefaultSymbolWithPointSize(kReadLaterActionSymbol,
                                              kSymbolActionPointSize);
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)
                         image:image
                          type:MenuActionType::AddToReadingList
                         block:block];
}

- (UIAction*)actionToBookmarkWithBlock:(ProceduralBlock)block {
  UIImage* image = DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                              kSymbolActionPointSize);
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOBOOKMARKS)
                         image:image
                          type:MenuActionType::AddToBookmarks
                         block:block];
}

- (UIAction*)actionToEditBookmarkWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kEditActionSymbol, kSymbolActionPointSize);
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)
                image:image
                 type:MenuActionType::EditBookmark
                block:block];
}

- (UIAction*)actionToCloseRegularTabWithBlock:(ProceduralBlock)block {
  NSString* title = l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSETAB);
  return [self actionToCloseTabWithTitle:title block:block];
}

- (UIAction*)actionToClosePinnedTabWithBlock:(ProceduralBlock)block {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSEPINNEDTAB);
  return [self actionToCloseTabWithTitle:title block:block];
}

- (UIAction*)actionToCloseAllOtherTabsWithBlock:(ProceduralBlock)block {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSEOTHERTABS);
  UIImage* image =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  UIAction* action = [self actionWithTitle:title
                                     image:image
                                      type:MenuActionType::CloseAllOtherTabs
                                     block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionSaveImageWithBlock:(ProceduralBlock)block {
  UIImage* image = DefaultSymbolWithPointSize(kSaveImageActionSymbol,
                                              kSymbolActionPointSize);
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SAVEIMAGE)
                image:image
                 type:MenuActionType::SaveImage
                block:block];
  return action;
}

- (UIAction*)actionCopyImageWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kCopyActionSymbol, kSymbolActionPointSize);
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPYIMAGE)
                image:image
                 type:MenuActionType::CopyImage
                block:block];
  return action;
}

- (UIAction*)actionSearchImageWithTitle:(NSString*)title
                                  Block:(ProceduralBlock)block {
  UIImage* image = CustomSymbolWithPointSize(kPhotoBadgeMagnifyingglassSymbol,
                                             kSymbolActionPointSize);
  UIAction* action = [self actionWithTitle:title
                                     image:image
                                      type:MenuActionType::SearchImage
                                     block:block];
  return action;
}

- (UIAction*)actionToCloseAllTabsWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  int titleID = IsTabGroupSyncEnabled()
                    ? IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABSANDGROUPS
                    : IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABS;
  UIAction* action = [self actionWithTitle:l10n_util::GetNSString(titleID)
                                     image:image
                                      type:MenuActionType::CloseAllTabs
                                     block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToSelectTabsWithBlock:(ProceduralBlock)block {
  UIImage* image = DefaultSymbolWithPointSize(kCheckmarkCircleSymbol,
                                              kSymbolActionPointSize);
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SELECTTABS)
                image:image
                 type:MenuActionType::SelectTabs
                block:block];
  return action;
}

- (UIAction*)actionToSearchImageUsingLensWithBlock:(ProceduralBlock)block {
  UIImage* image =
      CustomSymbolWithPointSize(kCameraLensSymbol, kSymbolActionPointSize);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTEXT_MENU_SEARCHIMAGEWITHGOOGLE)
                      image:image
                       type:MenuActionType::SearchImageWithLens
                      block:block];
  return action;
}

- (ProceduralBlock)recordMobileWebContextMenuOpenTabActionWithBlock:
    (ProceduralBlock)block {
  return ^{
    base::RecordAction(base::UserMetricsAction("MobileWebContextMenuOpenTab"));
    if (block) {
      block();
    }
  };
}

- (UIAction*)actionToAddTabsToNewGroupWithTabsNumber:(int)tabsNumber
                                           inSubmenu:(BOOL)inSubmenu
                                               block:(ProceduralBlock)block {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group context menu action "
         "outside the Tab Groups experiment.";
  UIImage* image = DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                              kSymbolActionPointSize);
  NSString* title =
      inSubmenu ? l10n_util::GetNSString(
                      IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU)
                : l10n_util::GetPluralNSStringF(
                      IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, tabsNumber);
  UIAction* action = [self actionWithTitle:title
                                     image:image
                                      type:MenuActionType::AddTabToNewGroup
                                     block:block];
  return action;
}

- (UIAction*)actionToOpenLinkInNewGroupWithBlock:(ProceduralBlock)block
                                       inSubmenu:(BOOL)inSubmenu {
  UIImage* image = DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                              kSymbolActionPointSize);
  NSString* title =
      inSubmenu ? l10n_util::GetNSString(
                      IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU)
                : l10n_util::GetNSString(
                      IDS_IOS_CONTENT_CONTEXT_OPENLINKINNEWTABGROUP);
  UIAction* action = [self actionWithTitle:title
                                     image:image
                                      type:MenuActionType::OpenLinkInNewGroup
                                     block:block];
  return action;
}

- (UIMenuElement*)
    menuToAddTabToGroupWithGroups:(const std::set<const TabGroup*>&)groups
                     numberOfTabs:(int)tabsNumber
                            block:(void (^)(const TabGroup*))block {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group context menu action "
         "outside the Tab Groups experiment.";

  if (groups.size() == 0) {
    ProceduralBlock addTabToNewGroupBlock = ^{
      if (block) {
        block(nil);
      }
    };
    return [self actionToAddTabsToNewGroupWithTabsNumber:tabsNumber
                                               inSubmenu:NO
                                                   block:addTabToNewGroupBlock];
  }

  NSArray<UIMenuElement*>* groupsMenu = [self groupsMenuForGroups:groups
                                                     currentGroup:nil
                                                            block:block];
  UIMenu* menu = [UIMenu menuWithTitle:@""
                                 image:nil
                            identifier:nil
                               options:UIMenuOptionsDisplayInline
                              children:groupsMenu];
  ProceduralBlock addTabToNewGroupBlock = ^{
    if (block) {
      block(nil);
    }
  };
  NSArray<UIMenuElement*>* addToGroupMenuElements = @[
    [self actionToAddTabsToNewGroupWithTabsNumber:tabsNumber
                                        inSubmenu:YES
                                            block:addTabToNewGroupBlock],
    menu
  ];

  UIImage* image = DefaultSymbolWithPointSize(kMoveTabToGroupActionSymbol,
                                              kSymbolActionPointSize);

  return [UIMenu
      menuWithTitle:l10n_util::GetPluralNSStringF(
                        IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP, tabsNumber)
              image:image
         identifier:nil
            options:UIMenuOptionsSingleSelection
           children:addToGroupMenuElements];
}

- (UIMenuElement*)
    menuToMoveTabToGroupWithGroups:(const std::set<const TabGroup*>&)groups
                      currentGroup:(const TabGroup*)currentGroup
                         moveBlock:(void (^)(const TabGroup*))moveBlock
                       removeBlock:(ProceduralBlock)removeBlock {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group context menu action "
         "outside the Tab Groups experiment.";

  if (groups.size() == 0) {
    NOTREACHED() << "Groups cannot be empty.";
  }

  NSArray<UIMenuElement*>* groupsMenu = [self groupsMenuForGroups:groups
                                                     currentGroup:currentGroup
                                                            block:moveBlock];
  UIMenu* menu = [UIMenu menuWithTitle:@""
                                 image:nil
                            identifier:nil
                               options:UIMenuOptionsDisplayInline
                              children:groupsMenu];
  NSArray<UIMenuElement*>* moveTabFromGroupMenuElements =
      @[ [self actionToRemoveTabFromGroup:removeBlock], menu ];

  UIImage* image = DefaultSymbolWithPointSize(kMoveTabToGroupActionSymbol,
                                              kSymbolActionPointSize);
  return [UIMenu menuWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_MOVETABTOGROUP)
                         image:image
                    identifier:nil
                       options:UIMenuOptionsSingleSelection
                      children:moveTabFromGroupMenuElements];
}

- (UIMenuElement*)
    menuToOpenLinkInGroupWithGroups:(const std::set<const TabGroup*>&)groups
                              block:(void (^)(const TabGroup*))block {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group context menu action "
         "outside the Tab Groups experiment.";

  if (groups.size() == 0) {
    ProceduralBlock openInNewGroupBlock = ^{
      if (block) {
        block(nil);
      }
    };
    return [self actionToOpenLinkInNewGroupWithBlock:openInNewGroupBlock
                                           inSubmenu:NO];
  }

  NSArray<UIMenuElement*>* groupsMenu = [self groupsMenuForGroups:groups
                                                     currentGroup:nil
                                                            block:block];
  UIMenu* menu = [UIMenu menuWithTitle:@""
                                 image:nil
                            identifier:nil
                               options:UIMenuOptionsDisplayInline
                              children:groupsMenu];
  ProceduralBlock openInNewGroupBlock = ^{
    if (block) {
      block(nil);
    }
  };
  NSArray<UIMenuElement*>* openInGroupMenuElements = @[
    [self actionToOpenLinkInNewGroupWithBlock:openInNewGroupBlock
                                    inSubmenu:YES],
    menu
  ];

  UIImage* image = DefaultSymbolWithPointSize(kMoveTabToGroupActionSymbol,
                                              kSymbolActionPointSize);

  return [UIMenu menuWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKINTABGROUP)
                         image:image
                    identifier:nil
                       options:UIMenuOptionsSingleSelection
                      children:openInGroupMenuElements];
}

- (UIAction*)actionToRenameTabGroupWithBlock:(ProceduralBlock)block {
  CHECK(IsTabGroupInGridEnabled());
  UIImage* image =
      DefaultSymbolWithPointSize(kEditActionSymbol, kSymbolActionPointSize);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP)
                      image:image
                       type:MenuActionType::RenameTabGroup
                      block:block];
  return action;
}

- (UIAction*)actionToAddNewTabInGroupWithBlock:(ProceduralBlock)block {
  CHECK(IsTabGroupInGridEnabled());
  UIImage* image = DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                              kSymbolActionPointSize);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_NEWTABINGROUP)
                      image:image
                       type:MenuActionType::NewTabInGroup
                      block:block];
  return action;
}

- (UIAction*)actionToUngroupTabGroupWithBlock:(ProceduralBlock)block {
  CHECK(IsTabGroupInGridEnabled());
  UIImage* image = DefaultSymbolWithPointSize(kUngroupTabGroupSymbol,
                                              kSymbolActionPointSize);
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_UNGROUP)
                image:image
                 type:MenuActionType::UngroupTabGroup
                block:block];
  return action;
}

- (UIAction*)actionToDeleteTabGroupWithBlock:(ProceduralBlock)block {
  CHECK(IsTabGroupInGridEnabled());
  UIImage* image =
      DefaultSymbolWithPointSize(kDeleteActionSymbol, kSymbolActionPointSize);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)
                      image:image
                       type:MenuActionType::DeleteTabGroup
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToCloseTabGroupWithBlock:(ProceduralBlock)block {
  CHECK(IsTabGroupInGridEnabled());
  CHECK(IsTabGroupSyncEnabled());

  UIImage* image =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSEGROUP)
                image:image
                 type:MenuActionType::CloseTabGroup
                block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

#pragma mark - Private

// Creates a UIAction instance for closing a tab with a provided `title`.
- (UIAction*)actionToCloseTabWithTitle:(NSString*)title
                                 block:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  UIAction* action = [self actionWithTitle:title
                                     image:image
                                      type:MenuActionType::CloseTab
                                     block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

// Creates a UIAction instance for removing a tab from a group.
- (UIAction*)actionToRemoveTabFromGroup:(ProceduralBlock)block {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group context menu action "
         "outside the Tab Groups experiment.";
  UIImage* image = DefaultSymbolWithPointSize(kRemoveTabFromGroupActionSymbol,
                                              kSymbolActionPointSize);
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_REMOVEFROMGROUP);
  UIAction* action = [self actionWithTitle:title
                                     image:image
                                      type:MenuActionType::RemoveTabFromGroup
                                     block:block];
  return action;
}

- (UIAction*)actionToShowDetailsWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolActionPointSize);
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_SHOW_DETAILS_ACTION_TITLE)
                image:image
                 type:MenuActionType::ShowDetails
                block:block];
}

// Returns an array of group actions for a given set of groups. If
// `currentGroup` is specified and is present in the set, it is selected.
- (NSArray<UIMenuElement*>*)
    groupsMenuForGroups:(const std::set<const TabGroup*>&)groups
           currentGroup:(const TabGroup*)currentGroup
                  block:(void (^)(const TabGroup*))block {
  NSMutableArray<UIMenuElement*>* groupsMenu = [[NSMutableArray alloc] init];

  UIImage* circleImage =
      DefaultSymbolWithPointSize(kCircleFillSymbol, kSymbolActionPointSize);
  circleImage =
      [circleImage imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  for (const TabGroup* group : groups) {
    NSString* title = group->GetTitle();
    base::WeakPtr<const TabGroup> weakGroup = group->GetWeakPtr();
    ProceduralBlock actionBlock = ^{
      if (block) {
        block(weakGroup.get());
      }
    };

    UIAction* groupAction =
        [self actionWithTitle:title
                        image:[circleImage imageWithTintColor:group->GetColor()]
                         type:MenuActionType::MoveTabToExistingGroup
                        block:actionBlock];

    if (group == currentGroup) {
      groupAction.state = UIMenuElementStateOn;
    }
    [groupsMenu addObject:groupAction];
  }
  return groupsMenu;
}

- (UIAction*)actionToSortDriveItemsByNameWithBlock:(ProceduralBlock)block {
  return
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_DRIVE_SORT_BY_NAME)
                      image:nil
                       type:MenuActionType::SortDriveItemsByName
                      block:block];
}

- (UIAction*)actionToSortDriveItemsByModificationTimeWithBlock:
    (ProceduralBlock)block {
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_DRIVE_SORT_BY_MODIFICATION)
                image:nil
                 type:MenuActionType::SortDriveItemsByModificationTime
                block:block];
}

- (UIAction*)actionToSortDriveItemsByOpeningTimeWithBlock:
    (ProceduralBlock)block {
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_DRIVE_SORT_BY_OPENING)
                image:nil
                 type:MenuActionType::SortDriveItemsByOpeningTime
                block:block];
}

- (UIMenuElement*)
    menuToSelectDriveIdentityWithIdentities:
        (NSArray<id<SystemIdentity>>*)identities
                            currentIdentity:(id<SystemIdentity>)currentIdentity
                                      block:(void (^)(const id<SystemIdentity>))
                                                block {
  NSMutableArray<UIMenuElement*>* identitiesMenuElements =
      [[NSMutableArray alloc] init];
  for (id<SystemIdentity> identity in identities) {
    NSString* email = identity.userEmail;
    ProceduralBlock actionBlock = ^{
      if (block) {
        block(identity);
      }
    };

    UIAction* identityAction =
        [self actionWithTitle:email
                        image:nil
                         type:MenuActionType::SelectDriveIdentity
                        block:actionBlock];
    if (identity == currentIdentity) {
      identityAction.state = UIMenuElementStateOn;
    }
    [identitiesMenuElements addObject:identityAction];
  }

  return [UIMenu menuWithTitle:@""
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:identitiesMenuElements];
}

- (UIAction*)actionToAddAccountForDriveWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_DRIVE_ADD_ACCOUNT)
                         image:nil
                          type:MenuActionType::AddDriveAccount
                         block:block];
}

- (UIAction*)actionToManageLinkInNewTabWithBlock:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kExternalLinkSymbol, kSymbolActionPointSize);

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENMANAGEINNEWTAB)
                         image:image
                          type:MenuActionType::ManageInNewTab
                         block:block];
}

- (UIAction*)actionToShowRecentActivity:(ProceduralBlock)block {
  UIImage* image =
      DefaultSymbolWithPointSize(kHistorySymbol, kSymbolActionPointSize);
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_RECENTACTIVITY)
                         image:image
                          type:MenuActionType::RecentActivityInSharedTabGroup
                         block:block];
}

@end
