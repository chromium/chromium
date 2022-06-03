// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_ITEM_THUMBNAIL_GENERATOR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_ITEM_THUMBNAIL_GENERATOR_H_

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

namespace web {
class WebState;
}

// ChromeActivityItemThumbnailGenerator will be used to retrieve activity items
// thumbnail given WebState.
@interface ChromeActivityItemThumbnailGenerator : NSObject

// Default initializer. |webState| must not be nullptr.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

// ChromeActivityItemThumbnailGenerator must be created with a WebState.
- (instancetype)init NS_UNAVAILABLE;

// Returns a thumbnail at the specified size. May return nil.
- (UIImage*)thumbnailWithSize:(const CGSize&)size;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_ITEM_THUMBNAIL_GENERATOR_H_
