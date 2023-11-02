// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptState;

class ClipboardItem final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ClipboardItem* Create(
      const HeapVector<std::pair<String, ScriptPromise>>& items,
      ExceptionState& exception_state);

  explicit ClipboardItem(
      const HeapVector<std::pair<String, ScriptPromise>>& items);
  Vector<String> types() const;
  ScriptPromise getType(ScriptState* script_state,
                        const String& type,
                        ExceptionState& exception_state) const;

  const HeapVector<std::pair<String, ScriptPromise>>& GetItems() const {
    return items_;
  }

  // Returns the custom formats that have a "web " prefix.
  const Vector<String>& CustomFormats() const { return custom_format_items_; }

  void Trace(Visitor*) const override;

 private:
  HeapVector<std::pair<String, ScriptPromise>> items_;
  Vector<String> custom_format_items_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_
