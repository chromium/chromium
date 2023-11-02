// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_FAVICON_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_FAVICON_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, CWVFaviconType) {
  // Invalid icon type.
  CWVFaviconTypeInvalid = 0,
  // <link rel="icon" ...>.
  CWVFaviconTypeFavicon,
  // <link rel="apple-touch-icon" ...>.
  CWVFaviconTypeTouchIcon,
  // <link rel="apple-touch-icon-precomposed" ...>.
  CWVFaviconTypeTouchPrecomposedIcon,
  // Icon listed in a web manifest.
  CWVFaviconTypeWebManifestIcon,
};

// Encapsulates information about a fav icon.
CWV_EXPORT
@interface CWVFavicon : NSObject

// URL of the icon.
@property(nonatomic, copy, readonly) NSURL* URL;
// Type of icon.
@property(nonatomic, readonly) CWVFaviconType type;
// Boxed values of CGSizes.
// There may be multiple sizes if the |type| is CWVFaviconTypeFavicon.
@property(nonatomic, readonly) NSArray<NSValue*>* sizes;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_FAVICON_H_
