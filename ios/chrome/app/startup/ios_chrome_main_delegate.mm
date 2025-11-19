// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/ios_chrome_main_delegate.h"

#import "base/logging.h"
#import "third_party/skia/include/core/SkGraphics.h"

IOSChromeMainDelegate::IOSChromeMainDelegate() {}

IOSChromeMainDelegate::~IOSChromeMainDelegate() {}

void IOSChromeMainDelegate::BasicStartupComplete() {
  // Initialize Skia. On desktop this is made by content::BrowserMainRunnerImpl,
  // however web does not have a dependency on skia, so it is done as part of
  // Chrome initialisation on iOS.
  SkGraphics::Init();

  // Upstream wires up log file handling here based on flags; for now that's
  // not supported, and this is called just to handle vlog levels and patterns.
  // If redirecting to a file is ever needed, add it here (see
  // logging_chrome.cc for example code).
  logging::LoggingSettings log_settings;
  logging::InitLogging(log_settings);
}
