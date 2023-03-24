// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_INIT_IOS_BROWSER_MAIN_PARTS_H_
#define IOS_WEB_CONTENT_INIT_IOS_BROWSER_MAIN_PARTS_H_

#import "build/blink_buildflags.h"
#import "content/public/browser/browser_main_parts.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace web {

class WebMainParts;

class IOSBrowserMainParts : public content::BrowserMainParts {
 public:
  IOSBrowserMainParts();
  IOSBrowserMainParts(const IOSBrowserMainParts&) = delete;
  IOSBrowserMainParts& operator=(const IOSBrowserMainParts&) = delete;
  ~IOSBrowserMainParts() override;

  // BrowserMainParts implementation:
  int PreEarlyInitialization() override;
  void PostEarlyInitialization() override;
  void PreCreateMainMessageLoop() override;
  void PostCreateMainMessageLoop() override;
  int PreCreateThreads() override;
  void PostCreateThreads() override;
  int PreMainMessageLoopRun() override;

 private:
  std::unique_ptr<web::WebMainParts> parts_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_BROWSER_MAIN_PARTS_H_
