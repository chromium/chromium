// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_CHANGE_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/bindings/bigint.h"

namespace blink {

class ClipboardChangeEventInit;

// https://www.w3.org/TR/clipboard-apis/#clipboard-event-clipboardchange
// This event is fired when the system clipboard changes.
// It contains the available native mime types when the
// clipboard contents change.
class ClipboardChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ClipboardChangeEvent(const Vector<String>& types, const BigInt& change_id);
  explicit ClipboardChangeEvent(const ClipboardChangeEventInit* initializer);
  ~ClipboardChangeEvent() override;

  static ClipboardChangeEvent* Create(const Vector<String>& types,
                                      const BigInt& change_id) {
    return MakeGarbageCollected<ClipboardChangeEvent>(types, change_id);
  }

  static ClipboardChangeEvent* Create(
      const ClipboardChangeEventInit* initializer) {
    return MakeGarbageCollected<ClipboardChangeEvent>(initializer);
  }

  void Trace(Visitor*) const override;

  Vector<String> types() const;
  blink::BigInt changeId() const;

 private:
  Vector<String> types_;
  BigInt change_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_CHANGE_EVENT_H_
