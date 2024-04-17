// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_BACK_FORWARD_LIST_ITEM_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_BACK_FORWARD_LIST_ITEM_H_

#import <UIKit/UIKit.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// A equivalent of
// https://developer.apple.com/documentation/webkit/wkbackforwardlistitem
CWV_EXPORT
@interface CWVBackForwardListItem : NSObject

// The URL of the item.
@property(nonatomic, readonly) NSURL* URL;

// The title of the item.
@property(nonatomic, readonly, nullable) NSString* title;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_BACK_FORWARD_LIST_ITEM_H_
