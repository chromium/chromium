// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_UTIL_H_
#define IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_UTIL_H_

#import <WebKit/WebKit.h>

#import "ios/web/public/js_messaging/content_world.h"

namespace web {

// Returns the web::ContentWorld enum value for the given `content_world`.
ContentWorld ContentWorldIdentifierForWKContentWorld(
    WKContentWorld* content_world);

// Returns the WKContentWorld enum value for the given web::ContentWorld enum
// value.
WKContentWorld* WKContentWorldForContentWorldIdentifier(
    ContentWorld content_world);

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_UTIL_H_
