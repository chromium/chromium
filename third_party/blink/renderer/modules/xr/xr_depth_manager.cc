// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_depth_manager.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRDepthManager::XRDepthManager(base::PassKey<XRSession> pass_key,
                               XRSession* session)
    : session_(session) {}

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
      case device::mojom::blink::XRDepthData::Tag::DATA_STILL_VALID:
        // Stale depth buffer is still the most recent information we have.
        // Current API shape is not well-suited to return data pertaining to
        // older frames, so we just discard the data we previously got and will
        // not set the new one.
        break;
      case device::mojom::blink::XRDepthData::Tag::UPDATED_DEPTH_DATA:
        // We got new depth buffer - store the current depth data as a member.
        depth_data_ = std::move(depth_data->get_updated_depth_data());
        break;
    }
  }
}

XRDepthInformation* XRDepthManager::GetDepthInformation(
    const XRFrame* xr_frame) {
  if (!depth_data_) {
    return nullptr;
  }

  EnsureData();

  return MakeGarbageCollected<XRDepthInformation>(
      xr_frame, depth_data_->size, depth_data_->norm_texture_from_norm_view,
      data_);
}

void XRDepthManager::EnsureData() {
  DCHECK(depth_data_);

  if (data_) {
    return;
  }

  base::span<const uint16_t> pixel_data = base::make_span(
      reinterpret_cast<const uint16_t*>(depth_data_->pixel_data.data()),
      depth_data_->pixel_data.size() / 2);

  // Copy the underlying pixel data into DOMUint16Array:
  data_ = DOMUint16Array::Create(pixel_data.data(), pixel_data.size());
}

void XRDepthManager::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(data_);
}

}  // namespace blink
