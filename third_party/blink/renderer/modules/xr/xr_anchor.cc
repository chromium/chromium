// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_anchor.h"
#include "third_party/blink/renderer/modules/xr/xr_object_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace {

constexpr char kAnchorAlreadyDeleted[] =
    "Unable to access anchor properties, the anchor was already deleted.";

}

namespace blink {

XRAnchor::XRAnchor(uint64_t id,
                   XRSession* session,
                   const device::mojom::blink::XRAnchorData& anchor_data)
    : id_(id),
      is_deleted_(false),
      session_(session),
      mojo_from_anchor_(anchor_data.mojo_from_anchor) {
  DVLOG(3) << __func__ << ": id_=" << id_
           << ", anchor_data.mojo_from_anchor.has_value()="
           << anchor_data.mojo_from_anchor.has_value();
}

void XRAnchor::Update(const device::mojom::blink::XRAnchorData& anchor_data) {
  DVLOG(3) << __func__ << ": id_=" << id_ << ", is_deleted_=" << is_deleted_
           << ", anchor_data.mojo_from_anchor.has_value()="
           << anchor_data.mojo_from_anchor.has_value();

  if (is_deleted_) {
    return;
  }

  mojo_from_anchor_ = anchor_data.mojo_from_anchor;
}

uint64_t XRAnchor::id() const {
  return id_;
}

XRSpace* XRAnchor::anchorSpace(ExceptionState& exception_state) const {
  DVLOG(2) << __func__ << ": id_=" << id_ << ", is_deleted_=" << is_deleted_
           << " anchor_space_ is valid? " << !!anchor_space_;

  if (is_deleted_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kAnchorAlreadyDeleted);
    return nullptr;
  }

  if (!anchor_space_) {
    anchor_space_ =
        MakeGarbageCollected<XRObjectSpace<XRAnchor>>(session_, this);
  }

  return anchor_space_.Get();
}

device::mojom::blink::XRNativeOriginInformationPtr XRAnchor::NativeOrigin()
    const {
  return device::mojom::blink::XRNativeOriginInformation::NewAnchorId(
      this->id());
}

std::optional<gfx::Transform> XRAnchor::MojoFromObject() const {
  DVLOG(3) << __func__ << ": id_=" << id_;

  if (!mojo_from_anchor_) {
    DVLOG(3) << __func__ << ": id_=" << id_ << ", mojo_from_anchor_ is not set";
    return std::nullopt;
  }

  return mojo_from_anchor_->ToTransform();
}

void XRAnchor::Delete() {
  DVLOG(1) << __func__ << ": id_=" << id_ << ", is_deleted_=" << is_deleted_;

  if (!is_deleted_) {
    session_->xr()->xrEnvironmentProviderRemote()->DetachAnchor(id_);
    mojo_from_anchor_ = std::nullopt;
    anchor_space_ = nullptr;
  }

  is_deleted_ = true;
}

void XRAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(anchor_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
