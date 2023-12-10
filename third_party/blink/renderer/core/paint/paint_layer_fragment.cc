// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"

namespace blink {

void PaintLayerFragment::Trace(Visitor* visitor) const {
  visitor->Trace(fragment_data);
  visitor->Trace(physical_fragment);
}

}  // namespace blink
