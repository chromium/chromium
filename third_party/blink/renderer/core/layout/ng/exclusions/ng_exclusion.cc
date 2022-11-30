// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

void NGExclusionShapeData::Trace(Visitor* visitor) const {
  visitor->Trace(layout_box);
}

bool NGExclusion::operator==(const NGExclusion& other) const {
  return type == other.type && rect == other.rect &&
         shape_data == other.shape_data;
}

}  // namespace blink
