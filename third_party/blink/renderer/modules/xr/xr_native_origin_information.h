// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_NATIVE_ORIGIN_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_NATIVE_ORIGIN_INFORMATION_H_

#include <cstdint>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"

namespace blink {

class XRAnchor;
class XRInputSource;
class XRPlane;
class XRReferenceSpace;

// XRNativeOriginInformation carries all the information that is required to
// uniquely identify a native origin on the device side. Native origin roughly
// represents anything that is known and tracked by the device, for example
// anchors, planes, input sources, reference spaces.
class XRNativeOriginInformation {
 public:
  XRNativeOriginInformation(XRNativeOriginInformation&& other) = default;

  device::mojom::blink::XRNativeOriginInformationPtr ToMojo() const;

  static base::Optional<XRNativeOriginInformation> Create(
      const XRAnchor* anchor);
  static base::Optional<XRNativeOriginInformation> Create(
      const XRInputSource* input_source);
  static base::Optional<XRNativeOriginInformation> Create(const XRPlane* plane);
  static base::Optional<XRNativeOriginInformation> Create(
      const XRReferenceSpace* reference_space);

 private:
  enum class Type : int32_t { ReferenceSpace, InputSource, Anchor, Plane };

  XRNativeOriginInformation() = delete;
  XRNativeOriginInformation(const XRNativeOriginInformation& other) = delete;
  void operator=(const XRNativeOriginInformation& other) = delete;

  XRNativeOriginInformation(Type type, uint32_t input_source_id);
  XRNativeOriginInformation(Type type, uint64_t anchor_or_plane_id);
  XRNativeOriginInformation(
      Type type,
      device::mojom::XRReferenceSpaceCategory reference_space_type);

  const Type type_;

  const union {
    uint32_t input_source_id_;
    uint64_t anchor_or_plane_id_;
    device::mojom::XRReferenceSpaceCategory reference_space_category_;
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_NATIVE_ORIGIN_INFORMATION_H_
