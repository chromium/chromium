// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActionFactory ()

// Histogram to record executed actions.
@property(nonatomic, assign) const char* histogram;

@end

@implementation ActionFactory

- (instancetype)initWithScenario:(MenuScenario)scenario {
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

- (UIImage*)configuredSymbolNamed:(NSString*)symbolName
                     systemSymbol:(BOOL)systemSymbol {
  UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];
  if (systemSymbol) {
    return [UIImage systemImageNamed:symbolName
                   withConfiguration:configuration];
  }
  return [UIImage imageNamed:symbolName
                    inBundle:nil
           withConfiguration:configuration];
}

- (UIAction*)actionToCopyURL:(const GURL)URL {
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE)
                image:[UIImage imageNamed:@"copy_link_url"]
                 type:MenuActionType::CopyURL
                block:^{
                  StoreURLInPasteboard(URL);
                }];
}

- (UIAction*)actionToShareWithBlock:(ProceduralBlock)block {
  return
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL)
                      image:[UIImage imageNamed:@"share"]
                       type:MenuActionType::Share
                      block:block];
}

- (UIAction*)actionToDeleteWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                      image:[UIImage imageNamed:@"delete"]
                       type:MenuActionType::Delete
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToOpenInNewTabWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                         image:[UIImage imageNamed:@"open_in_new_tab"]
                          type:MenuActionType::OpenInNewTab
                         block:block];
}

- (UIAction*)actionToOpenAllTabsWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPEN_ALL_LINKS)
                         image:[UIImage systemImageNamed:@"plus"]
                          type:MenuActionType::OpenAllInNewTabs
                         block:block];
}

- (UIAction*)actionToRemoveWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_REMOVE_ACTION_TITLE)
                      image:[UIImage imageNamed:@"remove"]
                       type:MenuActionType::Remove
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToEditWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE)
                         image:[UIImage imageNamed:@"edit"]
                          type:MenuActionType::Edit
                         block:block];
}

- (UIAction*)actionToHideWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_HIDE_MENU_OPTION)
                      image:[UIImage imageNamed:@"remove"]
                       type:MenuActionType::Hide
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToMoveFolderWithBlock:(ProceduralBlock)block {
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                image:[UIImage imageNamed:@"move_folder"]
                 type:MenuActionType::Move
                block:block];
}

- (UIAction*)actionToMarkAsReadWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_READ_ACTION)
                         image:[UIImage imageNamed:@"mark_read"]
                          type:MenuActionType::Read
                         block:block];
}

- (UIAction*)actionToMarkAsUnreadWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION)
                         image:[UIImage imageNamed:@"remove"]
                          type:MenuActionType::Unread
                         block:block];
}

- (UIAction*)actionToOpenOfflineVersionInNewTabWithBlock:
    (ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON)
                         image:[UIImage imageNamed:@"offline"]
                          type:MenuActionType::ViewOffline
                         block:block];
}

- (UIAction*)actionToAddToReadingListWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)
                         image:[UIImage imageNamed:@"read_later"]
                          type:MenuActionType::AddToReadingList
                         block:block];
}

- (UIAction*)actionToBookmarkWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOBOOKMARKS)
                         image:[UIImage imageNamed:@"bookmark"]
                          type:MenuActionType::AddToBookmarks
                         block:block];
}

- (UIAction*)actionToEditBookmarkWithBlock:(ProceduralBlock)block {
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)
                image:[UIImage imageNamed:@"bookmark"]
                 type:MenuActionType::EditBookmark
                block:block];
}

- (UIAction*)actionToCloseTabWithBlock:(ProceduralBlock)block {
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSETAB)
                image:[UIImage imageNamed:@"close"]
                 type:MenuActionType::CloseTab
                block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionSaveImageWithBlock:(ProceduralBlock)block {
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SAVEIMAGE)
                image:[UIImage imageNamed:@"download"]
                 type:MenuActionType::SaveImage
                block:block];
  return action;
}

- (UIAction*)actionCopyImageWithBlock:(ProceduralBlock)block {
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPYIMAGE)
                image:[UIImage imageNamed:@"copy"]
                 type:MenuActionType::CopyImage
                block:block];
  return action;
}

- (UIAction*)actionSearchImageWithTitle:(NSString*)title
                                  Block:(ProceduralBlock)block {
  UIAction* action = [self actionWithTitle:title
                                     image:[UIImage imageNamed:@"search_image"]
                                      type:MenuActionType::SearchImage
                                     block:block];
  return action;
}

- (UIAction*)actionToCloseAllTabsWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABS)
                      image:[UIImage imageNamed:@"close"]
                       type:MenuActionType::CloseAllTabs
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToSelectTabsWithBlock:(ProceduralBlock)block {
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SELECTTABS)
                image:[UIImage imageNamed:@"select"]
                 type:MenuActionType::SelectTabs
                block:block];
  return action;
}

- (UIAction*)actionToSearchImageUsingLensWithBlock:(ProceduralBlock)block {
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTEXT_MENU_SEARCHIMAGEWITHLENS)
                      image:[UIImage imageNamed:@"lens_icon"]
                       type:MenuActionType::SearchImageWithLens
                      block:block];
  return action;
}

@end
