// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_PREVIEW_ELEMENT_INFO_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_PREVIEW_ELEMENT_INFO_INTERNAL_H_

#import <Foundation/Foundation.h>

#import "ios/web_view/public/cwv_preview_element_info.h"

NS_ASSUME_NONNULL_BEGIN

@interface CWVPreviewElementInfo ()

- (instancetype)initWithLinkURL:(NSURL*)linkURL NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_PREVIEW_ELEMENT_INFO_INTERNAL_H_
