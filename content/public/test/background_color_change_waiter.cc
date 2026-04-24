// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/background_color_change_waiter.h"

#include "content/public/browser/web_contents.h"

namespace content {

BackgroundColorChangeWaiter::BackgroundColorChangeWaiter(
    content::WebContents* web_contents,
    std::optional<SkColor> expected_color)
    : content::WebContentsObserver(web_contents),
      expected_color_(expected_color) {}

BackgroundColorChangeWaiter::~BackgroundColorChangeWaiter() = default;

void BackgroundColorChangeWaiter::Wait() {
  if (observed_) {
    return;
  }

  // If an expected color was specified and it already matches, return early.
  if (ColorMatches()) {
    return;
  }

  run_loop_.Run();
}

void BackgroundColorChangeWaiter::OnBackgroundColorChanged() {
  // When waiting for a specific color, keep waiting if it doesn't match yet.
  if (expected_color_.has_value() && !ColorMatches()) {
    return;
  }

  observed_ = true;
  if (run_loop_.running()) {
    run_loop_.Quit();
  }
}

bool BackgroundColorChangeWaiter::ColorMatches() const {
  if (!expected_color_.has_value()) {
    return false;
  }
  std::optional<SkColor> current = web_contents()->GetBackgroundColor();
  return current.has_value() && current.value() == expected_color_.value();
}

}  // namespace content
