// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_MANAGER_H_

#include <array>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/types/pass_key.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ExceptionState;
class XRCPUDepthInformation;
class XRWebGLDepthInformation;
class XRView;
class XRViewData;

// Helper class, used to separate the code related to depth buffer processing
// out of XRViewData.
class XRDepthManager : public GarbageCollected<XRDepthManager> {
 public:
  explicit XRDepthManager(
      base::PassKey<XRViewData> pass_key,
      const device::mojom::blink::XRDepthConfig& device_configuration);
  virtual ~XRDepthManager();

  void ProcessDepthInformation(device::mojom::blink::XRDepthDataPtr depth_data);

  XRCPUDepthInformation* GetCpuDepthInformation(
      const XRView* xr_view,
      ExceptionState& exception_state);

  XRWebGLDepthInformation* GetWebGLDepthInformation(
      const XRView* xr_view,
      ExceptionState& exception_state);

  void Trace(Visitor* visitor) const;

 private:
  const device::mojom::XRDepthUsage usage_;
  const device::mojom::XRDepthDataFormat data_format_;

  // Current depth data buffer.
  device::mojom::blink::XRDepthDataUpdatedPtr depth_data_;

  // Optional persistent shared memory regions for double-buffering.
  std::array<base::UnsafeSharedMemoryRegion, 2> shared_buffer_regions_;

  // Cached DOMArrayBuffers mapping the shared memory regions in
  // `shared_buffer_regions_`.
  std::array<Member<DOMArrayBuffer>, 2> data_buffers_;

  // Cached version of the depth buffer data. If not null, contains the same
  // information as `depth_data_.pixel_data` buffer (or maps
  // `shared_buffer_regions_` directly).
  Member<DOMArrayBuffer> data_;
  bool data_dirty_ = true;

  void EnsureData();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_MANAGER_H_
