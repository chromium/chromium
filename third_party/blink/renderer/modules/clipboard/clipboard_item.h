// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptState;

class ClipboardItem final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ClipboardItem* Create(
      const HeapVector<std::pair<String, Member<Blob>>>& items,
      ExceptionState& exception_state);
  explicit ClipboardItem(
      const HeapVector<std::pair<String, Member<Blob>>>& items);
  Vector<String> types() const;
  ScriptPromise getType(ScriptState* script_state, const String& type) const;

  const HeapVector<std::pair<String, Member<Blob>>>& GetItems() const {
    return items_;
  }

  void Trace(blink::Visitor*) override;

 private:
  HeapVector<std::pair<String, Member<Blob>>> items_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_
