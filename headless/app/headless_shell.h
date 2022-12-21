// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_APP_HEADLESS_SHELL_H_
#define HEADLESS_APP_HEADLESS_SHELL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace headless {

class HeadlessBrowser;
class HeadlessBrowserContext;

// An application which implements a simple headless browser.
class HeadlessShell {
 public:
  HeadlessShell();

  HeadlessShell(const HeadlessShell&) = delete;
  HeadlessShell& operator=(const HeadlessShell&) = delete;

  ~HeadlessShell();

  void OnBrowserStart(HeadlessBrowser* browser);

 private:
  void ShutdownSoon();
  void Shutdown();

  raw_ptr<HeadlessBrowser> browser_ = nullptr;  // Not owned.
  raw_ptr<HeadlessBrowserContext> browser_context_ = nullptr;

  base::WeakPtrFactory<HeadlessShell> weak_factory_{this};
};

}  // namespace headless

#endif  // HEADLESS_APP_HEADLESS_SHELL_H_
