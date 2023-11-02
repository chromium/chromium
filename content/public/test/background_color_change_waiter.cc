// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/background_color_change_waiter.h"

namespace content {

BackgroundColorChangeWaiter::BackgroundColorChangeWaiter(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

BackgroundColorChangeWaiter::~BackgroundColorChangeWaiter() = default;

void BackgroundColorChangeWaiter::Wait() {
  if (observed_)
    return;

  run_loop_.Run();
}

void BackgroundColorChangeWaiter::OnBackgroundColorChanged() {
  observed_ = true;
  if (run_loop_.running())
    run_loop_.Quit();
}

}  // namespace content
