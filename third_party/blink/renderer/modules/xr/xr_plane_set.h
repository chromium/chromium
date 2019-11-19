// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SET_H_

#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/xr_setlike.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRPlaneSet : public ScriptWrappable, public XRSetlike<XRPlane> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRPlaneSet(HeapHashSet<Member<XRPlane>> planes);

  void Trace(blink::Visitor* visitor) override;

 protected:
  const HeapHashSet<Member<XRPlane>>& elements() const override;

 private:
  HeapHashSet<Member<XRPlane>> planes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_SET_H_
