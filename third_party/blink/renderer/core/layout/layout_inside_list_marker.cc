// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_inside_list_marker.h"

namespace blink {

LayoutInsideListMarker::LayoutInsideListMarker(Element* element)
    : LayoutInline(element) {}

LayoutInsideListMarker::~LayoutInsideListMarker() = default;

}  // namespace blink
