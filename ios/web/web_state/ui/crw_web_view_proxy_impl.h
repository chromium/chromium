// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_PROXY_IMPL_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_PROXY_IMPL_H_

#import <UIKit/UIKit.h>

#import "ios/web/common/crw_content_view.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

@class CRWWebController;

// TODO(crbug.com/546152): Rename class to CRWContentViewProxyImpl.
@interface CRWWebViewProxyImpl : NSObject<CRWWebViewProxy>

// Used by CRWWebController to set the content view being managed.
// |contentView|'s scroll view property will be managed by the
// WebViewScrollViewProxy.
@property(nonatomic, weak) CRWContentView* contentView;

// Init with a weak reference of web controller, used for passing through calls.
- (instancetype)initWithWebController:(CRWWebController*)webController;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_PROXY_IMPL_H_
