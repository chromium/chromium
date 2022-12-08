// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_size.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

ResizeObserverSize* ResizeObserverSize::Create(double inline_size,
                                               double block_size) {
  return MakeGarbageCollected<ResizeObserverSize>(inline_size, block_size);
}

ResizeObserverSize::ResizeObserverSize(double inline_size, double block_size)
    : inline_size_(inline_size), block_size_(block_size) {}

ResizeObserverSize::ResizeObserverSize() = default;

void ResizeObserverSize::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
