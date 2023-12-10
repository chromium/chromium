// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface ActionFactory ()

// Histogram to record executed actions.
@property(nonatomic, assign) const char* histogram;

@end

@implementation ActionFactory

- (instancetype)initWithScenario:(MenuScenarioHistogram)scenario {
  if (self = [super init]) {
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
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABS)
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

@end
