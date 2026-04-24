// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACKGROUND_COLOR_CHANGE_WAITER_H_
#define CONTENT_PUBLIC_TEST_BACKGROUND_COLOR_CHANGE_WAITER_H_

#include <optional>

#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

// Spins a run loop until the |web_contents|' HTML background color changes.
// An optional |expected_color| may be provided; if so, the waiter exits early
// when the current background color already matches, and only quits waiting
// when the background color changes to the expected value.
class BackgroundColorChangeWaiter : public content::WebContentsObserver {
 public:
  explicit BackgroundColorChangeWaiter(
      content::WebContents* web_contents,
      std::optional<SkColor> expected_color = std::nullopt);
  BackgroundColorChangeWaiter(const BackgroundColorChangeWaiter&) = delete;
  BackgroundColorChangeWaiter& operator=(const BackgroundColorChangeWaiter&) =
      delete;
  ~BackgroundColorChangeWaiter() override;

  void Wait();

  // content::WebContentsObserver:
  void OnBackgroundColorChanged() override;

 private:
  bool ColorMatches() const;

  std::optional<SkColor> expected_color_;
  bool observed_ = false;
  base::RunLoop run_loop_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACKGROUND_COLOR_CHANGE_WAITER_H_
