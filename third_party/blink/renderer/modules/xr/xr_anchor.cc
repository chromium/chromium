// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_anchor.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_object_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRAnchor::XRAnchor(uint64_t id, XRSession* session)
    : id_(id), session_(session), anchor_data_(base::nullopt) {}

XRAnchor::XRAnchor(uint64_t id,
                   XRSession* session,
                   const device::mojom::blink::XRAnchorDataPtr& anchor_data,
                   double timestamp)
    : id_(id),
      session_(session),
      anchor_data_(base::in_place, anchor_data, timestamp) {}

void XRAnchor::Update(const device::mojom::blink::XRAnchorDataPtr& anchor_data,
                      double timestamp) {
  if (!anchor_data_) {
    anchor_data_ = AnchorData(anchor_data, timestamp);
  } else {
    *anchor_data_->pose_matrix_ =
        mojo::ConvertTo<blink::TransformationMatrix>(anchor_data->pose);
    anchor_data_->last_changed_time_ = timestamp;
  }
}

uint64_t XRAnchor::id() const {
  return id_;
}

XRSpace* XRAnchor::anchorSpace() const {
  if (!anchor_data_) {
    return nullptr;
  }

  if (!anchor_space_) {
    anchor_space_ =
        MakeGarbageCollected<XRObjectSpace<XRAnchor>>(session_, this);
  }

  return anchor_space_;
}

TransformationMatrix XRAnchor::poseMatrix() const {
  if (anchor_data_) {
    return *anchor_data_->pose_matrix_;
  }

  // |poseMatrix()| shouldn't be called by anyone except XRObjectSpace and if
  // XRObjectSpace already exists for this anchor, then anchor_data_ should also
  // exist for this anchor.
  NOTREACHED();
  return {};
}

double XRAnchor::lastChangedTime(bool& is_null) const {
  if (!anchor_data_) {
    is_null = true;
    return 0;
  }

  is_null = false;
  return anchor_data_->last_changed_time_;
}

void XRAnchor::detach() {
  session_->xr()->xrEnvironmentProviderRemote()->DetachAnchor(id_);
}

void XRAnchor::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(anchor_space_);
  ScriptWrappable::Trace(visitor);
}

XRAnchor::AnchorData::AnchorData(
    const device::mojom::blink::XRAnchorDataPtr& anchor_data,
    double timestamp)
    : pose_matrix_(std::make_unique<TransformationMatrix>(
          mojo::ConvertTo<blink::TransformationMatrix>(anchor_data->pose))),
      last_changed_time_(timestamp) {}

}  // namespace blink
