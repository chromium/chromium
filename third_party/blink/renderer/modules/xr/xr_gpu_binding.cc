// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_binding.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_gpu_projection_layer_init.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_projection_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_sub_image.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

XRGPUBinding* XRGPUBinding::Create(XRSession* session,
                                   GPUDevice* device,
                                   ExceptionState& exception_state) {
  DCHECK(session);
  DCHECK(device);

  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRGPUBinding for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  if (!session->immersive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRGPUBinding for an "
                                      "inline XRSession.");
    return nullptr;
  }

  if (device->destroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRGPUBinding with a "
                                      "destroyed WebGPU device.");
    return nullptr;
  }

  if (!device->adapter()->isXRCompatible()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "WebGPU device must be created by an XR compatible adapter in order to "
        "use with an immersive XRSession");
    return nullptr;
  }

  return MakeGarbageCollected<XRGPUBinding>(session, device);
}

XRGPUBinding::XRGPUBinding(XRSession* session, GPUDevice* device)
    : XRGraphicsBinding(session), device_(device) {}

XRProjectionLayer* XRGPUBinding::createProjectionLayer(
    const XRGPUProjectionLayerInit* init,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<XRGPUProjectionLayer>(this, init);
}

XRGPUSubImage* XRGPUBinding::getViewSubImage(XRProjectionLayer* layer,
                                             XRView* view,
                                             ExceptionState& exception_state) {
  if (!OwnsLayer(layer)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Layer was not created with this binding.");
    return nullptr;
  }

  NOTIMPLEMENTED();
  return nullptr;
}

String XRGPUBinding::getPreferredColorFormat() {
  return FromDawnEnum(GPU::preferred_canvas_format());
}

void XRGPUBinding::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  XRGraphicsBinding::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
