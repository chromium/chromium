// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/theme_change_waiter.h"

namespace content {

ThemeChangeWaiter::ThemeChangeWaiter(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

ThemeChangeWaiter::~ThemeChangeWaiter() = default;

void ThemeChangeWaiter::Wait() {
  if (observed_)
    return;

  run_loop_.Run();
}

void ThemeChangeWaiter::DidChangeThemeColor() {
  observed_ = true;
  if (run_loop_.running())
    run_loop_.Quit();
}

}  // namespace content
