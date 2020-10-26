// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_OBJECT_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_OBJECT_SPACE_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/xr/xr_native_origin_information.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRSession;

// Helper class that returns an XRSpace that tracks the position of object of
// type T (for example XRPlane, XRAnchor). The type T has to have a
// MojoFromObject() method, returning a base::Optional<TransformationMatrix>.
//
// If the object's MojoFromObject() method returns a base::nullopt, it means
// that the object is not localizable in the current frame (i.e. its pose is
// unknown) - the `frame.getPose(objectSpace, otherSpace)` will return null.
// That does not necessarily mean that object tracking is lost - it may be that
// the object's location will become known in subsequent frames.
template <typename T>
class XRObjectSpace : public XRSpace {
 public:
  explicit XRObjectSpace(XRSession* session, const T* object)
      : XRSpace(session), object_(object) {}

  base::Optional<TransformationMatrix> MojoFromNative() override {
    return object_->MojoFromObject();
  }

  base::Optional<device::mojom::blink::XRNativeOriginInformation> NativeOrigin()
      const override {
    return XRNativeOriginInformation::Create(object_);
  }

  bool IsStationary() const override {
    // Object spaces are considered stationary - they are supposed to remain
    // fixed relative to their surroundings (at least locally).
    return true;
  }

  std::string ToString() const override { return "XRObjectSpace"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(object_);
    XRSpace::Trace(visitor);
  }

 private:
  Member<const T> object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_OBJECT_SPACE_H_
