// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_BACK_FORWARD_LIST_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_BACK_FORWARD_LIST_INTERNAL_H_

#import "ios/web_view/public/cwv_back_forward_list.h"

#import "ios/web_view/internal/cwv_back_forward_list_item_internal.h"

NS_ASSUME_NONNULL_BEGIN

namespace web {
class NavigationManager;
}  // namespace web

// This class exists to avoid recreating a new NSArray<CWVBackForwardListItem*>
// when |backForwardList.{back|forward}List| is called. The content of the list
// might silently be changed, so if it were a real array it must be recreated
// every time, which might result in an extra O(n) factor unexpectedly on some
// normal code like this:
//
// list = webView.backForwardList;
// for (i = 0; i < list.backList.count; i++) {
//   <DO SOMETHING WITH list.backList[i]>; // It silently generates a new list.
// }
// And this would be O(n^2) which surely is not something expected.
//
// This class is an array-like wrapper (provides .count and [] operator).
@interface CWVBackForwardListItemArray ()

// The CWVBackForwardList that |self| belongs to.
@property(nonatomic, weak, readonly) CWVBackForwardList* list;

// |self| is backList if |isBackList| == YES, otherwise it is forwardList.
- (instancetype)initWithBackForwardList:(CWVBackForwardList*)list
                             isBackList:(BOOL)isBackList
    NS_DESIGNATED_INITIALIZER;

@end

@interface CWVBackForwardList ()

// The lower-level information provider of backForwardListItem.
@property(nonatomic, nullable) const web::NavigationManager* navigationManager;

// Returns the index of the |item| in the lower level web::NavigationManager.
// -1 means item not found.
- (int)internalIndexOfItem:(CWVBackForwardListItem*)item;

- (instancetype)initWithNavigationManager:
    (const web::NavigationManager*)navigationManager NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_BACK_FORWARD_LIST_INTERNAL_H_
