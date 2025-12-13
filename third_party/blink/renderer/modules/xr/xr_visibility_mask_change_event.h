// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VISIBILITY_MASK_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VISIBILITY_MASK_CHANGE_EVENT_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_visibility_mask_change_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

class XRVisibilityMaskChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static XRVisibilityMaskChangeEvent* Create(
      const AtomicString& type,
      XRVisibilityMaskChangeEventInit* init) {
    return MakeGarbageCollected<XRVisibilityMaskChangeEvent>(type, init);
  }

  static XRVisibilityMaskChangeEvent* Create(
      const AtomicString& type,
      XRSession* session,
      V8XREye eye,
      uint32_t index,
      const device::mojom::blink::XRVisibilityMaskPtr& visibility_mask);

  XRVisibilityMaskChangeEvent(const AtomicString& type,
                              XRVisibilityMaskChangeEventInit* init);

  XRVisibilityMaskChangeEvent(const AtomicString& type,
                              XRSession* session,
                              V8XREye eye,
                              uint32_t index,
                              DOMFloat32Array* vertices,
                              DOMUint32Array* indices);

  ~XRVisibilityMaskChangeEvent() override;

  XRSession* session() const { return session_.Get(); }
  V8XREye eye() const { return eye_; }
  uint32_t index() const { return index_; }
  NotShared<DOMFloat32Array> vertices() const {
    return NotShared<DOMFloat32Array>(vertices_.Get());
  }
  NotShared<DOMUint32Array> indices() const {
    return NotShared<DOMUint32Array>(indices_.Get());
  }

  void Trace(Visitor* visitor) const override;

 private:
  Member<XRSession> session_;
  V8XREye eye_;
  uint32_t index_;
  Member<DOMFloat32Array> vertices_;
  Member<DOMUint32Array> indices_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VISIBILITY_MASK_CHANGE_EVENT_H_
