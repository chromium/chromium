// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_item_ptr.h"

namespace blink {

void InlineItemPtr::Trace(Visitor* visitor) const {
  visitor->Trace(items_data_);
}

}  // namespace blink
