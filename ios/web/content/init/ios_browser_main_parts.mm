// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_browser_main_parts.h"

#import "ios/web/public/init/web_main_parts.h"
#import "ios/web/public/web_client.h"

namespace web {

IOSBrowserMainParts::IOSBrowserMainParts() {
  parts_ = web::GetWebClient()->CreateWebMainParts();
}

IOSBrowserMainParts::~IOSBrowserMainParts() {}

int IOSBrowserMainParts::PreEarlyInitialization() {
  parts_->PreEarlyInitialization();
  return 0;
}
void IOSBrowserMainParts::PostEarlyInitialization() {
  parts_->PostEarlyInitialization();
}
void IOSBrowserMainParts::PreCreateMainMessageLoop() {
  parts_->PreCreateMainMessageLoop();
}
void IOSBrowserMainParts::PostCreateMainMessageLoop() {
  parts_->PostCreateMainMessageLoop();
}

int IOSBrowserMainParts::PreCreateThreads() {
  parts_->PreCreateThreads();
  return 0;
}

void IOSBrowserMainParts::PostCreateThreads() {
  parts_->PostCreateThreads();
}

int IOSBrowserMainParts::PreMainMessageLoopRun() {
  parts_->PreMainMessageLoopRun();
  return 0;
}

}  // namespace web
