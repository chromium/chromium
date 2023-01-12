// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_
#define IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/functional/callback.h"

namespace web {

// Generates a PDF of the entire content of a `web_view` and invokes the
// `callback` with the NSData of the PDF.
void CreateFullPagePdf(WKWebView* web_view,
                       base::OnceCallback<void(NSData*)> callback);
}  // namespace web

#endif  // IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_
