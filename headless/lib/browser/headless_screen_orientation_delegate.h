// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_ORIENTATION_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_ORIENTATION_DELEGATE_H_

#include "content/public/browser/screen_orientation_delegate.h"

namespace content {
class WebContents;
}

namespace headless {

// Headless implementation of ScreenOrientationDelegate. The functionality of
// ScreenOrientationProvider is always supported.
class HeadlessScreenOrientationDelegate
    : public content::ScreenOrientationDelegate {
 public:
  HeadlessScreenOrientationDelegate();

  HeadlessScreenOrientationDelegate(const HeadlessScreenOrientationDelegate&) =
      delete;
  HeadlessScreenOrientationDelegate& operator=(
      const HeadlessScreenOrientationDelegate&) = delete;

  ~HeadlessScreenOrientationDelegate() override;

  // ScreenOrientationDelegate:
  bool FullScreenRequired(content::WebContents* web_contents) override;
  void Lock(content::WebContents* web_contents,
            device::mojom::ScreenOrientationLockType lock_orientation) override;
  bool ScreenOrientationProviderSupported(
      content::WebContents* web_contents) override;
  void Unlock(content::WebContents* web_contents) override;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_ORIENTATION_DELEGATE_H_
