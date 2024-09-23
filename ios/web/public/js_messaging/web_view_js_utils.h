// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_VIEW_JS_UTILS_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_VIEW_JS_UTILS_H_

#import <Foundation/Foundation.h>

#include <memory>

namespace base {
class Value;
}  // namespace base

namespace web {

// Converts result of WKWebView script evaluation to base::Value.
std::unique_ptr<base::Value> ValueResultFromWKResult(id result);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_VIEW_JS_UTILS_H_
