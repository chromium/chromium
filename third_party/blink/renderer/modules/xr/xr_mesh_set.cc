// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_mesh_set.h"

namespace blink {

XRMeshSet::XRMeshSet(HeapHashSet<Member<XRMesh>> meshes) : meshes_(meshes) {}

const HeapHashSet<Member<XRMesh>>& XRMeshSet::elements() const {
  return meshes_;
}

void XRMeshSet::Trace(Visitor* visitor) const {
  visitor->Trace(meshes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
