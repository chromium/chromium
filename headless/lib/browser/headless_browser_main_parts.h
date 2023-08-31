// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_export.h"

namespace headless {

class HeadlessBrowserImpl;

class HEADLESS_EXPORT HeadlessBrowserMainParts
    : public content::BrowserMainParts {
 public:
  explicit HeadlessBrowserMainParts(HeadlessBrowserImpl& browser);

  HeadlessBrowserMainParts(const HeadlessBrowserMainParts&) = delete;
  HeadlessBrowserMainParts& operator=(const HeadlessBrowserMainParts&) = delete;

  ~HeadlessBrowserMainParts() override;

  // content::BrowserMainParts implementation:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;
#if BUILDFLAG(IS_POSIX)
  void PostCreateMainMessageLoop() override;
#endif

 private:
  void MaybeStartLocalDevToolsHttpHandler();

  raw_ref<HeadlessBrowserImpl> browser_;

  bool devtools_http_handler_started_ = false;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
