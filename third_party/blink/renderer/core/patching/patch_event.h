// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_patch_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/patching/patch.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class PatchEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PatchEvent* Create(const AtomicString& type,
                            const PatchEventInit* init) {
    return MakeGarbageCollected<PatchEvent>(type, init->patch());
  }

  PatchEvent(const AtomicString& type, Patch* patch)
      : Event(type, Bubbles::kNo, Cancelable::kNo), patch_(patch) {}

  ~PatchEvent() override = default;

  const AtomicString& InterfaceName() const override {
    return event_interface_names::kPatchEvent;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(patch_);
    Event::Trace(visitor);
  }

  Patch* patch() const { return patch_; }

  bool IsPatchEvent() const override { return true; }

 private:
  Member<Patch> patch_;
};

template <>
struct DowncastTraits<PatchEvent> {
  static bool AllowFrom(const Event& event) { return event.IsPatchEvent(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_EVENT_H_
