// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// static
ClipboardItem* ClipboardItem::Create(
    const HeapVector<std::pair<String, Member<Blob>>>& items,
    ExceptionState& exception_state) {
  // Check that incoming dictionary isn't empty. If it is, it's possible that
  // Javascript bindings implicitly converted an Object (like a Blob) into {},
  // an empty dictionary.
  if (!items.size()) {
    exception_state.ThrowTypeError("Empty dictionary argument");
    return nullptr;
  }
  return MakeGarbageCollected<ClipboardItem>(items);
}

ClipboardItem::ClipboardItem(
    const HeapVector<std::pair<String, Member<Blob>>>& items)
    : items_(items) {}

Vector<String> ClipboardItem::types() const {
  Vector<String> types;
  types.ReserveInitialCapacity(items_.size());
  for (const auto& item : items_) {
    types.push_back(item.first);
  }
  return types;
}

ScriptPromise ClipboardItem::getType(ScriptState* script_state,
                                     const String& type) const {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  for (const auto& item : items_) {
    if (type == item.first) {
      resolver->Resolve(item.second);
      return promise;
    }
  }

  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotFoundError, "The type was not found"));
  return promise;
}

void ClipboardItem::Trace(blink::Visitor* visitor) {
  visitor->Trace(items_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
