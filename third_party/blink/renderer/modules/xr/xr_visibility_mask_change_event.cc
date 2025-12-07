// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_visibility_mask_change_event.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"

namespace blink {

// static
XRVisibilityMaskChangeEvent* XRVisibilityMaskChangeEvent::Create(
    const AtomicString& type,
    XRSession* session,
    V8XREye eye,
    uint32_t index,
    const device::mojom::blink::XRVisibilityMaskPtr& visibility_mask) {
  // The spec requires that the event send the vertices and indices; but if
  // there is no visibility_mask, they should be empty.
  DOMFloat32Array* vertices;
  DOMUint32Array* indices;
  if (visibility_mask) {
    // We need to flatten the vertices before sending to the page.
    vertices = DOMFloat32Array::Create(visibility_mask->vertices.size() * 2);
    auto vertices_span = vertices->AsSpan();
    auto mojom_span = visibility_mask->vertices;
    for (size_t i = 0; i < mojom_span.size(); i++) {
      vertices_span[2 * i] = mojom_span[i].x();
      vertices_span[2 * i + 1] = mojom_span[i].y();
    }

    // WebGL/WebGPU (and indeed any javascript usage), have their own failure
    // mechanisms if any of these indices are out of bounds for the vertices
    // list, so we don't need to do further validations here.
    indices = DOMUint32Array::Create(visibility_mask->unvalidated_indices);
  } else {
    vertices = DOMFloat32Array::Create(0);
    indices = DOMUint32Array::Create(0);
  }
  return MakeGarbageCollected<XRVisibilityMaskChangeEvent>(
      type, session, eye, index, vertices, indices);
}

XRVisibilityMaskChangeEvent::XRVisibilityMaskChangeEvent(
    const AtomicString& type,
    XRVisibilityMaskChangeEventInit* init)
    : Event(type, init),
      session_(init->session()),
      eye_(init->eye()),
      index_(init->index()),
      vertices_(init->vertices().Get()),
      indices_(init->indices().Get()) {}

XRVisibilityMaskChangeEvent::XRVisibilityMaskChangeEvent(
    const AtomicString& type,
    XRSession* session,
    V8XREye eye,
    uint32_t index,
    DOMFloat32Array* vertices,
    DOMUint32Array* indices)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      session_(session),
      eye_(eye),
      index_(index),
      vertices_(vertices),
      indices_(indices) {}

XRVisibilityMaskChangeEvent::~XRVisibilityMaskChangeEvent() = default;

void XRVisibilityMaskChangeEvent::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(vertices_);
  visitor->Trace(indices_);
  Event::Trace(visitor);
}

}  // namespace blink
