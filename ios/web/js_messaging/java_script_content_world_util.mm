// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world_util.h"

#import <ostream>

#import "base/notreached.h"
#import "ios/web/public/js_messaging/content_world.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContentWorld ContentWorldIdentifierForWKContentWorld(
    WKContentWorld* content_world) {
  if (content_world == WKContentWorld.pageWorld) {
    return ContentWorld::kPageContentWorld;
  }
  if (content_world == WKContentWorld.defaultClientWorld) {
    return ContentWorld::kIsolatedWorld;
  }
  NOTREACHED() << "Missing association of WKContentWorld instance to a "
               << "web::ContentWorld value.";
  return ContentWorld::kAllContentWorlds;
}

WKContentWorld* WKContentWorldForContentWorldIdentifier(
    ContentWorld content_world) {
  if (content_world == ContentWorld::kPageContentWorld) {
    return WKContentWorld.pageWorld;
  }
  if (content_world == ContentWorld::kIsolatedWorld) {
    return WKContentWorld.defaultClientWorld;
  }
  NOTREACHED() << "Missing association of web::ContentWorld value to a"
               << "WKContentWorld instance.";
  return nil;
}

}  // namespace web
