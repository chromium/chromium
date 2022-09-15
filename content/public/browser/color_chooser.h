// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COLOR_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_COLOR_CHOOSER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace content {

// Interface for a color chooser.
class ColorChooser {
 public:
  virtual ~ColorChooser() {}

  // Ends connection with color chooser. Closes color chooser depending on the
  // platform.
  virtual void End() = 0;

  // Sets the selected color.
  virtual void SetSelectedColor(SkColor color) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COLOR_CHOOSER_H_
