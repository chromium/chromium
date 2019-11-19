// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/ios_chrome_main_delegate.h"

#include "base/logging.h"
#include "components/component_updater/component_updater_paths.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "third_party/skia/include/core/SkGraphics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeMainDelegate::IOSChromeMainDelegate() {}

IOSChromeMainDelegate::~IOSChromeMainDelegate() {}

void IOSChromeMainDelegate::BasicStartupComplete() {
  // Initialize Skia. On desktop this is made by content::BrowserMainRunnerImpl,
  // however web does not have a dependency on skia, so it is done as part of
  // Chrome initialisation on iOS.
  SkGraphics::Init();

  // Initialize the Chrome path provider.
  ios::RegisterPathProvider();

  // Register the component updater path provider.
  // Bundled components are not supported on ios, so DIR_USER_DATA is passed
  // for all three arguments.
  component_updater::RegisterPathProvider(
      ios::DIR_USER_DATA, ios::DIR_USER_DATA, ios::DIR_USER_DATA);

  // Upstream wires up log file handling here based on flags; for now that's
  // not supported, and this is called just to handle vlog levels and patterns.
  // If redirecting to a file is ever needed, add it here (see
  // logging_chrome.cc for example code).
  logging::LoggingSettings log_settings;
  logging::InitLogging(log_settings);
}
