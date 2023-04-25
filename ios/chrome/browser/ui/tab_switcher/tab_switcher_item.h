// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_

#import <UIKit/UIKit.h>

@class TabSwitcherItem;

// Block invoked when an image fetching operation completes. The `image`
// is nil if the operation failed.
using TabSwitcherImageFetchingCompletionBlock =
    void (^)(TabSwitcherItem* identifier, UIImage* image);

// Model object representing an item in the tab switchers.
//
// This class declares image fetching methods but doesn't do any fetching.
// It calls the completion block synchronously with a nil image.
// Subclasses should override the image fetching methods.
// It is OK not to call this class' implementations.
@interface TabSwitcherItem : NSObject

// Create an item with `identifier`, which cannot be nil.
- (instancetype)initWithIdentifier:(NSString*)identifier
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, copy, readonly) NSString* identifier;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) BOOL hidesTitle;
@property(nonatomic, assign) BOOL showsActivity;

#pragma mark - Image Fetching

// Fetches the favicon, calling `completion` on the calling sequence when the
// operation completes.
- (void)fetchFavicon:(TabSwitcherImageFetchingCompletionBlock)completion;

// Fetches the snapshot, calling `completion` on the calling sequence when the
// operation completes.
- (void)fetchSnapshot:(TabSwitcherImageFetchingCompletionBlock)completion;

// Prefetches the snapshot. Once the asynchronous fetch has returned, the next
// call to `fetchSnapshot:` can be synchronous.
- (void)prefetchSnapshot;

// Clears the potential prefetched snapshot.
- (void)clearPrefetchedSnapshot;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_
