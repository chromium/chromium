// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world_util.h"

#import <ostream>

#import "base/feature_list.h"
#import "base/notreached.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/content_world.h"

namespace web {

namespace {

WKContentWorld* IsolatedWKContentWorld() {
  // `WKContentWorldConfiguration` is only available in the iOS 27 SDK.
#if defined(__IPHONE_27_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_0
  if (@available(iOS 27, *)) {
    if (base::FeatureList::IsEnabled(
            features::kIsolatedWorldDisableLegacyBuiltinOverrides)) {
      static WKContentWorld* const configured_world = [] {
        WKContentWorldConfiguration* configuration =
            [[WKContentWorldConfiguration alloc] init];
        // Prevent DOM clobbering (e.g., `<form name="forms">`) from shadowing
        // built-in `Document` and `HTMLFormElement` properties in this world
        // while leaving the page world unaffected.
        configuration.legacyBuiltinOverridesEnabled = NO;
        return [WKContentWorld worldWithConfiguration:configuration];
      }();
      return configured_world;
    }
  }
#endif
  return WKContentWorld.defaultClientWorld;
}

}  // namespace

ContentWorld ContentWorldIdentifierForWKContentWorld(
    WKContentWorld* content_world) {
  if (content_world == WKContentWorld.pageWorld) {
    return ContentWorld::kPageContentWorld;
  }
  if (content_world == IsolatedWKContentWorld()) {
    return ContentWorld::kIsolatedWorld;
  }
  NOTREACHED() << "Missing association of WKContentWorld instance to a "
                  "web::ContentWorld value.";
}

WKContentWorld* WKContentWorldForContentWorldIdentifier(
    ContentWorld content_world) {
  if (content_world == ContentWorld::kPageContentWorld) {
    return WKContentWorld.pageWorld;
  }
  if (content_world == ContentWorld::kIsolatedWorld) {
    return IsolatedWKContentWorld();
  }
  NOTREACHED() << "Missing association of web::ContentWorld value to a "
                  "WKContentWorld instance.";
}

}  // namespace web
