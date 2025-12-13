// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_PLATFORM_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_PLATFORM_DELEGATE_H_

#include "headless/public/headless_browser.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Compositor;
}

namespace headless {
class HeadlessWebContentsImpl;

class HeadlessPlatformDelegate {
 public:
  HeadlessPlatformDelegate() = default;

  HeadlessPlatformDelegate(const HeadlessPlatformDelegate&) = delete;
  HeadlessPlatformDelegate& operator=(const HeadlessPlatformDelegate&) = delete;

  virtual ~HeadlessPlatformDelegate() = default;

  virtual void Initialize(const HeadlessBrowser::Options& options);
  virtual void Start();
  virtual void InitializeWebContents(HeadlessWebContentsImpl* web_contents);
  virtual void SetWebContentsBounds(HeadlessWebContentsImpl* web_contents,
                                    const gfx::Rect& bounds);
  virtual ui::Compositor* GetCompositor(HeadlessWebContentsImpl* web_contents);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_PLATFORM_DELEGATE_H_
