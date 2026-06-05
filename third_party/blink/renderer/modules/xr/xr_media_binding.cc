// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_media_binding.h"

#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRMediaBinding* XRMediaBinding::Create(ScriptState* script_state,
                                       XRSession* session,
                                       ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRMediaBinding for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }
  return MakeGarbageCollected<XRMediaBinding>(session);
}

XRMediaBinding::XRMediaBinding(XRSession* session) : session_(session) {}

XRQuadLayer* XRMediaBinding::createQuadLayer(HTMLVideoElement* video,
                                             const XRMediaQuadLayerInit* init,
                                             ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented yet.");
  return nullptr;
}

XRCylinderLayer* XRMediaBinding::createCylinderLayer(
    HTMLVideoElement* video,
    const XRMediaCylinderLayerInit* init,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented yet.");
  return nullptr;
}

XREquirectLayer* XRMediaBinding::createEquirectLayer(
    HTMLVideoElement* video,
    const XRMediaEquirectLayerInit* init,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented yet.");
  return nullptr;
}

void XRMediaBinding::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
