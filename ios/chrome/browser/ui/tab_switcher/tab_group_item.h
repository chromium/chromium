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
#endif

// Block invoked when a GroupTabInfo fetching operation completes. The
// `groupTabInfos` is nil if the operation failed.
typedef void (^GroupTabInfosFetchingCompletionBlock)(
    TabGroupItem* item,
    NSArray<GroupTabInfo*>* groupTabInfos);

// Model object representing an group item.
@interface TabGroupItem : NSObject

#ifdef __cplusplus
- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup
    NS_DESIGNATED_INITIALIZER;
#endif
- (instancetype)init NS_UNAVAILABLE;

#ifdef __cplusplus
@property(nonatomic, readonly) const TabGroup* tabGroup;
#endif
@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) UIColor* groupColor;

// Fetches the groupTabInfos (pair of snapshots and favicons), calling
// `completion` on the calling sequence when the operation completes.
- (void)fetchGroupTabInfos:(GroupTabInfosFetchingCompletionBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GROUP_ITEM_H_
