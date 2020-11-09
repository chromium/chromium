// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_
#define IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/callback.h"

namespace web {

// Returns true if workaround for loading restricted URLs should be applied.
// TODO(crbug.com/954332): Remove this workaround when iOS 12 is dropped.
// https://bugs.webkit.org/show_bug.cgi?id=196930 is fixed.
bool RequiresContentFilterBlockingWorkaround();

// Returns true if this iOS version is known to have
// https://bugs.webkit.org/show_bug.cgi?id=198794 WebKit bug.
// TODO(crbug.com/973653): Remove this workaround when WebKit bug is fixed.
bool RequiresProvisionalNavigationFailureWorkaround();

// Generates a PDF of the entire content of a |web_view| and invokes the
// |callback| with the NSData of the PDF.
void CreateFullPagePdf(WKWebView* web_view,
                       base::OnceCallback<void(NSData*)> callback);
}  // namespace web

#endif  // IOS_WEB_WEB_VIEW_WK_WEB_VIEW_UTIL_H_
