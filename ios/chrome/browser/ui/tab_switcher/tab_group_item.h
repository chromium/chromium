// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GROUP_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GROUP_ITEM_H_

#import <UIKit/UIKit.h>

@class TabGroupItem;

@class GroupTabInfo;
#ifdef __cplusplus
class TabGroup;
class WebStateList;
#endif

// Block invoked when a GroupTabInfo fetching operation completes. The
// `groupTabInfos` is nil if the operation failed.
typedef void (^GroupTabInfosFetchingCompletionBlock)(
    TabGroupItem* _Nullable item,
    NSArray<GroupTabInfo*>* _Nullable groupTabInfos);

// Model object representing an group item.
@interface TabGroupItem : NSObject

#ifdef __cplusplus
- (nonnull instancetype)initWithTabGroup:(const TabGroup* _Nonnull)tabGroup
                             webStateList:(WebStateList* _Nonnull)webStateList
    NS_DESIGNATED_INITIALIZER;
#endif
- (nonnull instancetype)init NS_UNAVAILABLE;

#ifdef __cplusplus
// Identifier of the group. Use this to compare two TabGroupItem objects.
@property(nonatomic, readonly, nonnull) const void* tabGroupIdentifier;
// If the underlying `TabGroup` object still exists, returns it.
// Otherwise, returns `nullptr`.
@property(nonatomic, readonly, nullable) const TabGroup* tabGroup;
#endif
@property(nonatomic, readonly, nullable) NSString* title;
@property(nonatomic, readonly, nullable) UIColor* groupColor;
@property(nonatomic, readonly, nullable) UIColor* foregroundColor;
@property(nonatomic, readonly) NSInteger numberOfTabsInGroup;
@property(nonatomic, readonly) BOOL collapsed;

// Fetches the groupTabInfos (pair of snapshots and favicons), calling
// `completion` on the calling sequence when the operation completes.
- (void)fetchGroupTabInfos:
    (nonnull GroupTabInfosFetchingCompletionBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GROUP_ITEM_H_
