// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_THEME_CHANGE_WAITER_H_
#define CONTENT_PUBLIC_TEST_THEME_CHANGE_WAITER_H_

#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// Spins a run loop until the |web_contents|' HTML theme color changes.
class ThemeChangeWaiter : public content::WebContentsObserver {
 public:
  explicit ThemeChangeWaiter(content::WebContents* web_contents);
  ~ThemeChangeWaiter() override;

  void Wait();

  // content::WebContentsObserver:
  void DidChangeThemeColor() override;

 private:
  bool observed_ = false;
  base::RunLoop run_loop_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_THEME_CHANGE_WAITER_H_
