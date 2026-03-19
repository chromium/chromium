// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_mesh.h"

#include "base/containers/span.h"
#include "base/types/optional_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_object_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "base/containers/span.h"


namespace blink {

namespace {
DOMFloat32Array* ConvertVerticesToFloat32Array(
    const Vector<float>& vertices) {
  if (vertices.empty()) {
    return DOMFloat32Array::Create(0);
  }
  return DOMFloat32Array::Create(vertices);
}

DOMUint32Array* ConvertIndicesToUint32Array(const Vector<uint32_t>& indices) {
  if (indices.empty()) {
    return DOMUint32Array::Create(0);
  }
  return DOMUint32Array::Create(indices);
}
}  // namespace

XRMesh::XRMesh(Id id,
               XRSession* session,
               const device::mojom::blink::XRMeshData& mesh_data,
               double timestamp)
    : XRMesh(id,
             session,
             ConvertVerticesToFloat32Array(mesh_data.vertices),
             ConvertIndicesToUint32Array(mesh_data.indices),
             SemanticLabelToString(mesh_data.semantic_label),
             base::OptionalToPtr(mesh_data.mojo_from_mesh),
             timestamp) {}

XRMesh::XRMesh(Id id,
               XRSession* session,
               DOMFloat32Array* vertices,
               DOMUint32Array* indices,
               const String& semantic_label,
               const device::Pose* mojo_from_mesh,
               double timestamp)
    : id_(id),
      vertices_(vertices),
      indices_(indices),
      semantic_label_(semantic_label),
      mojo_from_mesh_(base::OptionalFromPtr(mojo_from_mesh)),
      session_(session),
      last_changed_time_(timestamp){}

XRMesh::Id XRMesh::id() const {
  return id_;
}

XRSpace* XRMesh::meshSpace() const {
  if (!mesh_space_) {
    mesh_space_ = MakeGarbageCollected<XRObjectSpace<XRMesh>>(session_, this);
  }

  return mesh_space_.Get();
}

std::optional<gfx::Transform> XRMesh::MojoFromObject() const {
  if (!mojo_from_mesh_) {
    return std::nullopt;
  }

  return mojo_from_mesh_->ToTransform();
}

device::mojom::blink::XRNativeOriginInformationPtr XRMesh::NativeOrigin()
    const {
  return device::mojom::blink::XRNativeOriginInformation::NewMeshId(this->id());
}

const String& XRMesh::semanticLabel() const {
  return semantic_label_;
}

NotShared<DOMFloat32Array> XRMesh::vertices() const {
  return NotShared<DOMFloat32Array>(vertices_);
}

NotShared<DOMUint32Array> XRMesh::indices() const {
  return NotShared<DOMUint32Array>(indices_);
}

double XRMesh::lastChangedTime() const {
  return last_changed_time_;
}

void XRMesh::Update(const device::mojom::blink::XRMeshData& mesh_data,
                    double timestamp) {
  DVLOG(3) << __func__;

  last_changed_time_ = timestamp;

  mojo_from_mesh_ = mesh_data.mojo_from_mesh;

  vertices_ = ConvertVerticesToFloat32Array(mesh_data.vertices);
  indices_ = ConvertIndicesToUint32Array(mesh_data.indices);
  semantic_label_ = SemanticLabelToString(mesh_data.semantic_label);
}

void XRMesh::Trace(Visitor* visitor) const {
  visitor->Trace(mesh_space_);
  visitor->Trace(vertices_);
  visitor->Trace(indices_);
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
