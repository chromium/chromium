// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_VISIBILITY_MASK_HANDLER_H_
#define DEVICE_VR_OPENXR_OPENXR_VISIBILITY_MASK_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "device/vr/public/mojom/visibility_mask_id.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionEnumeration;
class OpenXrExtensionHelper;

class OpenXrVisibilityMaskHandler {
 public:
  static const char* GetExtension();
  static bool IsSupported(const OpenXrExtensionEnumeration& extension_enum);

  OpenXrVisibilityMaskHandler(const OpenXrExtensionHelper& extension_helper,
                              XrSession session);
  ~OpenXrVisibilityMaskHandler();

  void OnVisibilityMaskChanged(
      const XrEventDataVisibilityMaskChangedKHR& event);

  void UpdateVisibilityMaskData(XrViewConfigurationType type,
                                uint32_t view_index,
                                mojom::XRViewPtr& view);

 private:
  struct CachedMask {
    CachedMask();
    ~CachedMask();
    CachedMask(CachedMask&&);
    CachedMask& operator=(CachedMask&&);

    mojom::XRVisibilityMaskPtr mask;
    XrVisibilityMaskId id;
  };
  using ViewIndexMap = absl::flat_hash_map<uint32_t, CachedMask>;

  void UpdateVisibilityMask(XrViewConfigurationType type,
                            uint32_t view_index,
                            CachedMask& visibility_mask);

  raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrVisibilityMaskId::Generator visibility_mask_id_generator_;

  absl::flat_hash_map<XrViewConfigurationType, ViewIndexMap> visibility_masks_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_VISIBILITY_MASK_HANDLER_H_
