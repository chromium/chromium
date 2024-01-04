// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_

#import <UIKit/UIKit.h>

@class TabSwitcherItem;

#ifdef __cplusplus
class GURL;
namespace web {
class WebStateID;
}  // namespace web
#endif

// Block invoked when an image fetching operation completes. The `image`
// is nil if the operation failed.
typedef void (^TabSwitcherImageFetchingCompletionBlock)(TabSwitcherItem* item,
                                                        UIImage* image);

// Model object representing an item in the tab switchers.
//
// This class declares image fetching methods but doesn't do any fetching.
// It calls the completion block synchronously with a nil image.
// Subclasses should override the image fetching methods.
// It is OK not to call this class' implementations.
@interface TabSwitcherItem : NSObject

#ifdef __cplusplus
// Create an item with `identifier`, which cannot be nil.
- (instancetype)initWithIdentifier:(web::WebStateID)identifier
    NS_DESIGNATED_INITIALIZER;
#endif
- (instancetype)init NS_UNAVAILABLE;

#ifdef __cplusplus
@property(nonatomic, readonly) web::WebStateID identifier;
@property(nonatomic, assign) GURL URL;
#endif
@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) BOOL hidesTitle;
@property(nonatomic, readonly) BOOL showsActivity;

#pragma mark - Image Fetching

// Fetches the favicon, calling `completion` on the calling sequence when the
// operation completes.
- (void)fetchFavicon:(TabSwitcherImageFetchingCompletionBlock)completion;

// Fetches the snapshot, calling `completion` on the calling sequence when the
// operation completes.
- (void)fetchSnapshot:(TabSwitcherImageFetchingCompletionBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_
