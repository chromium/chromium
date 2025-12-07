// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_visibility_mask_handler.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/visibility_mask_id.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace device {

// static
const char* OpenXrVisibilityMaskHandler::GetExtension() {
  return XR_KHR_VISIBILITY_MASK_EXTENSION_NAME;
}

// static
bool OpenXrVisibilityMaskHandler::IsSupported(
    const OpenXrExtensionEnumeration& extension_enum) {
  return extension_enum.ExtensionSupported(GetExtension());
}

OpenXrVisibilityMaskHandler::CachedMask::CachedMask() = default;
OpenXrVisibilityMaskHandler::CachedMask::~CachedMask() = default;
OpenXrVisibilityMaskHandler::CachedMask::CachedMask(CachedMask&&) = default;
OpenXrVisibilityMaskHandler::CachedMask&
OpenXrVisibilityMaskHandler::CachedMask::operator=(CachedMask&&) = default;

OpenXrVisibilityMaskHandler::OpenXrVisibilityMaskHandler(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session)
    : extension_helper_(extension_helper), session_(session) {
  DCHECK(IsSupported(*extension_helper_->ExtensionEnumeration()));
}

OpenXrVisibilityMaskHandler::~OpenXrVisibilityMaskHandler() = default;

void OpenXrVisibilityMaskHandler::OnVisibilityMaskChanged(
    const XrEventDataVisibilityMaskChangedKHR& event) {
  auto type = event.viewConfigurationType;
  auto view_index = event.viewIndex;
  // We only care about events for visibility masks we've previously queried.
  auto* type_map = base::FindOrNull(visibility_masks_, type);
  if (!type_map) {
    return;
  }

  auto* visibility_mask = base::FindOrNull(*type_map, view_index);
  if (!visibility_mask) {
    return;
  }

  UpdateVisibilityMask(type, view_index, *visibility_mask);
}

void OpenXrVisibilityMaskHandler::UpdateVisibilityMaskData(
    XrViewConfigurationType type,
    uint32_t view_index,
    mojom::XRViewPtr& view) {
  // The operator[] returns a reference of the value type, creating it if it
  // needs to be created. Try Emplace won't create a new object if one already
  // existed however.
  auto [mask_it, inserted] = visibility_masks_[type].try_emplace(view_index);
  auto& mask = mask_it->second;
  if (inserted) {
    UpdateVisibilityMask(type, view_index, mask);
  }

  view->visibility_mask = mask.mask ? mask.mask.Clone() : nullptr;
  view->visibility_mask_id = mask.id;
}

void OpenXrVisibilityMaskHandler::UpdateVisibilityMask(
    XrViewConfigurationType type,
    uint32_t view_index,
    CachedMask& visibility_mask) {
  // This object shouldn't have been created unless our extension is supported,
  // and if our extension is supported this function pointer should exist.
  PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR =
      extension_helper_->ExtensionMethods().xrGetVisibilityMaskKHR;
  CHECK(xrGetVisibilityMaskKHR);

  // Reset the mask to null with a new ID. If our update fails, we still want to
  // indicate that we tried.
  visibility_mask.mask = nullptr;
  visibility_mask.id = visibility_mask_id_generator_.GenerateNextId();

  XrVisibilityMaskKHR mask_info = {XR_TYPE_VISIBILITY_MASK_KHR};
  XrResult result = xrGetVisibilityMaskKHR(
      session_, type, view_index,
      XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, &mask_info);
  if (XR_FAILED(result)) {
    return;
  }

  // If either vertex or indices array will be empty, then we've already setup
  // an empty mask.
  if (mask_info.vertexCountOutput == 0 || mask_info.indexCountOutput == 0) {
    return;
  }

  std::vector<XrVector2f> vertices(mask_info.vertexCountOutput);
  std::vector<uint32_t> indices(mask_info.indexCountOutput);

  mask_info.vertexCapacityInput = vertices.size();
  mask_info.vertices = vertices.data();
  mask_info.indexCapacityInput = indices.size();
  mask_info.indices = indices.data();

  result = xrGetVisibilityMaskKHR(
      session_, type, view_index,
      XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, &mask_info);
  if (XR_FAILED(result)) {
    return;
  }

  visibility_mask.mask = mojom::XRVisibilityMask::New();

  // OpenXR returns a set of XrVector2f's, but blink (and by extension mojom)
  // wants that flattened, so we'll actually need double the size of vertices
  // that we currently have. Since we're going to access everything by index,
  // resize is fine here.
  visibility_mask.mask->vertices.reserve(vertices.size());
  for (const auto& vertex : vertices) {
    visibility_mask.mask->vertices.emplace_back(vertex.x, vertex.y);
  }

  visibility_mask.mask->unvalidated_indices = std::move(indices);
}

}  // namespace device
