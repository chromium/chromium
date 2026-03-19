// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MESH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MESH_H_

#include <optional>

#include "device/vr/public/mojom/mesh_id.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class XRSession;
class XRSpace;

class XRMesh : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using Id = device::MeshId;

  XRMesh(Id id,
         XRSession* session,
         const device::mojom::blink::XRMeshData& mesh_data,
         double timestamp);

  Id id() const;

  XRSpace* meshSpace() const;

  // Device-to-app: returns the transform from this mesh's local coordinate
  // space to the internal mojo coordinate space, or nullopt if the pose is
  // unknown. Consumed by getPose() to compute the mesh's position relative to
  // any app-facing XRSpace: refSpace_from_mojo * mojo_from_mesh.
  std::optional<gfx::Transform> MojoFromObject() const;

  // App-to-device: returns a native origin identifier (the mesh's MeshId)
  // that tells the device layer which coordinate system the app is referring
  // to. Sent over Mojo IPC when the app requests operations relative to this
  // mesh's space (e.g., anchor creation, hit test subscription).
  device::mojom::blink::XRNativeOriginInformationPtr NativeOrigin() const;

  NotShared<DOMFloat32Array> vertices() const;
  NotShared<DOMUint32Array> indices() const;
  double lastChangedTime() const;
  const String& semanticLabel() const;

  void Update(const device::mojom::blink::XRMeshData& mesh_data,
              double timestamp);

  bool IsStationary() const {
    // Mesh objects are considered stationary since their poses don't vary
    // dramatically from frame to frame. Note: there is no mesh pose
    // correction after drift correction yet; IsStationary() may need to return
    // false once that is implemented.
    return true;
  }

  void Trace(Visitor* visitor) const override;

 private:
  XRMesh(Id id,
         XRSession* session,
         DOMFloat32Array* vertices,
         DOMUint32Array* indices,
         const String& semantic_label,
         const device::Pose* mojo_from_mesh,
         double timestamp);

  const Id id_;
  Member<DOMFloat32Array> vertices_;
  Member<DOMUint32Array> indices_;
  String semantic_label_;

  std::optional<device::Pose> mojo_from_mesh_;

  Member<XRSession> session_;

  double last_changed_time_;

  mutable Member<XRSpace> mesh_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_MESH_H_
