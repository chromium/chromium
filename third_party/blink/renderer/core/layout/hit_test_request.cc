// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

void HitTestRequest::Trace(Visitor* visitor) const {
  visitor->Trace(stop_node_);
}

}  // namespace blink
