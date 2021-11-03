// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_

#include "third_party/blink/renderer/core/css/media_values_dynamic.h"

namespace blink {

class CSSContainerValues : public MediaValuesDynamic {
 public:
  explicit CSSContainerValues(Document& document, double width, double height);

  double Width() const override { return width_; }
  double Height() const override { return height_; }

 protected:
  double width_;
  double height_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_
