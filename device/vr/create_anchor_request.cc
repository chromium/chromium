// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/create_anchor_request.h"

namespace device {

CreateAnchorRequest::CreateAnchorRequest(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform native_origin_from_anchor,
    CreateAnchorCallback callback)
    : native_origin_information_(native_origin_information.Clone()),
      native_origin_from_anchor_(native_origin_from_anchor),
      request_start_time_(base::TimeTicks::Now()),
      callback_(std::move(callback)) {}
CreateAnchorRequest::CreateAnchorRequest(CreateAnchorRequest&& other) = default;
CreateAnchorRequest::~CreateAnchorRequest() = default;

const mojom::XRNativeOriginInformation&
CreateAnchorRequest::GetNativeOriginInformation() const {
  return *native_origin_information_;
}

const gfx::Transform& CreateAnchorRequest::GetNativeOriginFromAnchor() const {
  return native_origin_from_anchor_;
}

const base::TimeTicks& CreateAnchorRequest::GetRequestStartTime() const {
  return request_start_time_;
}

CreateAnchorCallback CreateAnchorRequest::TakeCallback() {
  return std::move(callback_);
}

}  // namespace device
