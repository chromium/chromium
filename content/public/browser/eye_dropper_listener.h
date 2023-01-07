// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_EYE_DROPPER_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_EYE_DROPPER_LISTENER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace content {

// Callback interface to receive the color selection from the eye dropper.
class EyeDropperListener {
 public:
  virtual ~EyeDropperListener() = default;

  virtual void ColorSelected(SkColor color) = 0;
  virtual void ColorSelectionCanceled() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_EYE_DROPPER_LISTENER_H_
