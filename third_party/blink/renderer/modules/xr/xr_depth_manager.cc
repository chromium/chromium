// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_depth_manager.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/modules/xr/xr_cpu_depth_information.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace {

constexpr char kInvalidUsageMode[] =
    "Unable to obtain XRCPUDepthInformation in \"gpu-optimized\" usage mode.";

}  // namespace

namespace blink {

XRDepthManager::XRDepthManager(
    base::PassKey<XRViewData> pass_key,
    const device::mojom::blink::XRDepthConfig& depth_configuration)
    : usage_(depth_configuration.depth_usage),
      data_format_(depth_configuration.depth_data_format) {
  DVLOG(3) << __func__ << ": usage_=" << usage_
           << ", data_format_=" << data_format_;
}

XRDepthManager::~XRDepthManager() = default;

void XRDepthManager::ProcessDepthInformation(
    device::mojom::blink::XRDepthDataPtr depth_data) {
  DVLOG(3) << __func__ << ": depth_data valid? " << !!depth_data;

  // Throw away old data, we won't need it anymore because we'll either replace
  // it with new data, or no new data is available (& we don't want to keep the
  // old data in that case as well).
  depth_data_ = nullptr;
  data_ = nullptr;

  if (depth_data) {
    DVLOG(3) << __func__ << ": depth_data->which()="
             << static_cast<uint32_t>(depth_data->which());

    switch (depth_data->which()) {
      case device::mojom::blink::XRDepthData::Tag::kDataStillValid:
        // Stale depth buffer is still the most recent information we have.
        // Current API shape is not well-suited to return data pertaining to
        // older frames, so we just discard the data we previously got and will
        // not set the new one.
        break;
      case device::mojom::blink::XRDepthData::Tag::kUpdatedDepthData:
        // We got new depth buffer - store the current depth data as a member.
        depth_data_ = std::move(depth_data->get_updated_depth_data());
        break;
    }
  }
}

XRCPUDepthInformation* XRDepthManager::GetCpuDepthInformation(
    const XRFrame* xr_frame,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (usage_ != device::mojom::XRDepthUsage::kCPUOptimized) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidUsageMode);
    return nullptr;
  }

  if (!depth_data_) {
    return nullptr;
  }

  EnsureData();

  return MakeGarbageCollected<XRCPUDepthInformation>(
      xr_frame, depth_data_->size, depth_data_->norm_texture_from_norm_view,
      depth_data_->raw_value_to_meters, data_format_, data_);
}

XRWebGLDepthInformation* XRDepthManager::GetWebGLDepthInformation(
    const XRFrame* xr_frame,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (usage_ != device::mojom::XRDepthUsage::kGPUOptimized) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidUsageMode);
    return nullptr;
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void XRDepthManager::EnsureData() {
  DCHECK(depth_data_);

  if (data_) {
    return;
  }

  // Copy the pixel data into ArrayBuffer:
  data_ = DOMArrayBuffer::Create(depth_data_->pixel_data);
}

void XRDepthManager::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
}

}  // namespace blink
