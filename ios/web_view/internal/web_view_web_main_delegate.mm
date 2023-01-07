// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_delegate.h"

#include "base/base_paths.h"
#include "base/logging.h"
#import "base/mac/bundle_locations.h"
#include "components/component_updater/component_updater_paths.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Dummy class used to locate the containing NSBundle.
@interface CWVBundleLocator : NSObject
@end

@implementation CWVBundleLocator
@end

namespace ios_web_view {

WebViewWebMainDelegate::WebViewWebMainDelegate() {}

WebViewWebMainDelegate::~WebViewWebMainDelegate() = default;

void WebViewWebMainDelegate::BasicStartupComplete() {
  base::mac::SetOverrideFrameworkBundle(
      [NSBundle bundleForClass:[CWVBundleLocator class]]);

  // Sets up logging so logging levels can be controlled.
  logging::InitLogging(logging::LoggingSettings());

  component_updater::RegisterPathProvider(
      base::DIR_APP_DATA, base::DIR_APP_DATA, base::DIR_APP_DATA);
}

}  // namespace ios_web_view
