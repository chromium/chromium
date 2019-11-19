// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_accessibility_delegate.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - ReadingListCustomAction

// A custom item subclass that holds a reference to its ListItem.
@interface ReadingListCustomAction : UIAccessibilityCustomAction

// The reading list item.
@property(nonatomic, readonly, strong) id<ReadingListListItem> item;

- (instancetype)initWithName:(NSString*)name
                      target:(id)target
                    selector:(SEL)selector
                        item:(id<ReadingListListItem>)item
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithName:(NSString*)name
                      target:(id)target
                    selector:(SEL)selector NS_UNAVAILABLE;
@end

@implementation ReadingListCustomAction
@synthesize item = _item;

- (instancetype)initWithName:(NSString*)name
                      target:(id)target
                    selector:(SEL)selector
                        item:(id<ReadingListListItem>)item {
  if (self = [super initWithName:name target:target selector:selector]) {
    _item = item;
  }
  return self;
}

@end

#pragma mark - ReadingListListItemCustomActionFactory

@implementation ReadingListListItemCustomActionFactory
@synthesize accessibilityDelegate = _accessibilityDelegate;

- (NSArray<UIAccessibilityCustomAction*>*)customActionsForItem:
    (id<ReadingListListItem>)item {
  ReadingListCustomAction* toggleReadStatus = nil;
  if ([self.accessibilityDelegate isItemRead:item]) {
    toggleReadStatus = [[ReadingListCustomAction alloc]
        initWithName:l10n_util::GetNSString(
                         IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON)
              target:self
            selector:@selector(markUnread:)
                item:item];
  } else {
    toggleReadStatus = [[ReadingListCustomAction alloc]
        initWithName:l10n_util::GetNSString(
                         IDS_IOS_READING_LIST_MARK_READ_BUTTON)
              target:self
            selector:@selector(markRead:)
                item:item];
  }

  ReadingListCustomAction* openInNewTabAction = [[ReadingListCustomAction alloc]
      initWithName:l10n_util::GetNSString(
                       IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
            target:self
          selector:@selector(openInNewTab:)
              item:item];
  ReadingListCustomAction* openInNewIncognitoTabAction =
      [[ReadingListCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                target:self
              selector:@selector(openInNewIncognitoTab:)
                  item:item];
  ReadingListCustomAction* copyURLAction = [[ReadingListCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY)
            target:self
          selector:@selector(copyURL:)
              item:item];

  NSMutableArray* customActions = [NSMutableArray
      arrayWithObjects:toggleReadStatus, openInNewTabAction,
                       openInNewIncognitoTabAction, copyURLAction, nil];

  if (item.distillationState == ReadingListUIDistillationStatusSuccess) {
    // Add the possibility to open offline version only if the entry is
    // distilled.
    ReadingListCustomAction* openOfflineAction =
        [[ReadingListCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE)
                  target:self
                selector:@selector(openOffline:)
                    item:item];

    [customActions addObject:openOfflineAction];
  }

  return customActions;
}

- (BOOL)markRead:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate markItemRead:action.item];
  return YES;
}

- (BOOL)markUnread:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate markItemUnread:action.item];
  return YES;
}

- (BOOL)openInNewTab:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate openItemInNewTab:action.item];
  return YES;
}

- (BOOL)openInNewIncognitoTab:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate openItemInNewIncognitoTab:action.item];
  return YES;
}

- (BOOL)copyURL:(ReadingListCustomAction*)action {
  StoreURLInPasteboard(action.item.entryURL);
  return YES;
}

- (BOOL)openOffline:(ReadingListCustomAction*)action {
  [self.accessibilityDelegate openItemOffline:action.item];
  return YES;
}

@end
