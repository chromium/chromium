// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_UTIL_WK_WEB_VIEW_UTIL_H_
#define IOS_WEB_UTIL_WK_WEB_VIEW_UTIL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <optional>

#import "base/functional/callback.h"

namespace web {

// Generates a PDF of `web_view` for `document_rect` (or the entire page when
// `std::nullopt`) and invokes `callback` with the NSData of the PDF, or `nil`
// if `web_view` is nil, `document_rect` is invalid, or PDF generation fails.
void CreatePdf(WKWebView* web_view,
               std::optional<CGRect> document_rect,
               base::OnceCallback<void(NSData*)> callback);

}  // namespace web

#endif  // IOS_WEB_UTIL_WK_WEB_VIEW_UTIL_H_
