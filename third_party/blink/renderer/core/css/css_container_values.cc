// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_values.h"

#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSContainerValues::CSSContainerValues(Document& document,
                                       double width,
                                       double height)
    : MediaValuesDynamic(document.GetFrame()), width_(width), height_(height) {}

}  // namespace blink
