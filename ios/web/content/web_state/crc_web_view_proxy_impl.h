// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_WEB_STATE_CRC_WEB_VIEW_PROXY_IMPL_H_
#define IOS_WEB_CONTENT_WEB_STATE_CRC_WEB_VIEW_PROXY_IMPL_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/ui/crw_web_view_proxy.h"

#import "build/blink_buildflags.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

@interface CRCWebViewProxyImpl : NSObject <CRWWebViewProxy>

@property(nonatomic, weak) UIScrollView* contentView;

- (instancetype)init;

@end

#endif  // IOS_WEB_CONTENT_WEB_STATE_CRC_WEB_VIEW_PROXY_IMPL_H_
