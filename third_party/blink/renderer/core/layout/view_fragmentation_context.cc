// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/view_fragmentation_context.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

void ViewFragmentationContext::Trace(Visitor* visitor) const {
  visitor->Trace(view_);
  FragmentationContext::Trace(visitor);
}

}  // namespace blink
