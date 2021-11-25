// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_item_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
ClipboardItem* ClipboardItem::Create(
    const HeapVector<std::pair<String, ScriptPromise>>& items,
    const ClipboardItemOptions* options,
    ExceptionState& exception_state) {
  DCHECK(options);
  // Check that incoming dictionary isn't empty. If it is, it's possible that
  // Javascript bindings implicitly converted an Object (like a ScriptPromise)
  // into {}, an empty dictionary.
  if (!items.size()) {
    exception_state.ThrowTypeError("Empty dictionary argument");
    return nullptr;
  }
  return MakeGarbageCollected<ClipboardItem>(items, options);
}

ClipboardItem::ClipboardItem(
    const HeapVector<std::pair<String, ScriptPromise>>& items,
    const ClipboardItemOptions* options)
    : items_(items) {
  DCHECK(items_.size());
  if (options->hasUnsanitized()) {
    for (const auto& unsanitized_item : options->unsanitized()) {
      custom_format_items_.push_back(unsanitized_item);
    }
  }
}

Vector<String> ClipboardItem::types() const {
  Vector<String> types;
  types.ReserveInitialCapacity(items_.size());
  for (const auto& item : items_) {
    types.push_back(item.first);
  }
  return types;
}

ScriptPromise ClipboardItem::getType(ScriptState* script_state,
                                     const String& type,
                                     ExceptionState& exception_state) const {
  for (const auto& item : items_) {
    if (type == item.first) {
      return item.second;
    }
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                    "The type was not found");
  return ScriptPromise();
}

void ClipboardItem::Trace(Visitor* visitor) const {
  visitor->Trace(items_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
