// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_PREVIEW_ELEMENT_INFO_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_PREVIEW_ELEMENT_INFO_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// An object which contains information for previewing a webpage.
CWV_EXPORT
@interface CWVPreviewElementInfo : NSObject

// The link for the webpage to be previewed.
@property(nonatomic, readonly) NSURL* linkURL;

- (instancetype)initWithLinkURL:(NSURL*)linkURL NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_PREVIEW_ELEMENT_INFO_H_
