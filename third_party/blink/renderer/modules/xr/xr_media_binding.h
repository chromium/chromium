// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MEDIA_BINDING_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MEDIA_BINDING_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ExceptionState;
class HTMLVideoElement;
class ScriptState;
class XRSession;
class XRQuadLayer;
class XRMediaQuadLayerInit;
class XRCylinderLayer;
class XRMediaCylinderLayerInit;
class XREquirectLayer;
class XRMediaEquirectLayerInit;

class XRMediaBinding final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRMediaBinding(XRSession*);
  ~XRMediaBinding() override = default;

  static XRMediaBinding* Create(ScriptState* script_state,
                                XRSession* session,
                                ExceptionState& exception_state);

  XRQuadLayer* createQuadLayer(HTMLVideoElement* video,
                               const XRMediaQuadLayerInit* init,
                               ExceptionState&);
  XRCylinderLayer* createCylinderLayer(HTMLVideoElement* video,
                                       const XRMediaCylinderLayerInit* init,
                                       ExceptionState&);
  XREquirectLayer* createEquirectLayer(HTMLVideoElement* video,
                                       const XRMediaEquirectLayerInit* init,
                                       ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  Member<XRSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MEDIA_BINDING_H_
