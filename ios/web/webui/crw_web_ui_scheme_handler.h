// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_CRW_WEB_UI_SCHEME_HANDLER_H_
#define IOS_WEB_WEBUI_CRW_WEB_UI_SCHEME_HANDLER_H_

#import <WebKit/WebKit.h>

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// This class allows the handle the pages associated with a WebUI URL, using the
// custom scheme handling of the WKWebView.
@interface CRWWebUISchemeHandler : NSObject <WKURLSchemeHandler>

// Initializes the handler with the `URLLoaderFactory` used to load the URLs.
- (instancetype)initWithURLLoaderFactory:
    (scoped_refptr<network::SharedURLLoaderFactory>)URLLoaderFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_WEB_WEBUI_CRW_WEB_UI_SCHEME_HANDLER_H_
