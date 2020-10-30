// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_NATIVE_ORIGIN_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_NATIVE_ORIGIN_INFORMATION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"

namespace blink {

class XRAnchor;
class XRImageTrackingResult;
class XRInputSource;
class XRLightProbe;
class XRPlane;
class XRReferenceSpace;

namespace XRNativeOriginInformation {

device::mojom::blink::XRNativeOriginInformation Create(const XRAnchor* anchor);
device::mojom::blink::XRNativeOriginInformation Create(
    const XRImageTrackingResult* image);
device::mojom::blink::XRNativeOriginInformation Create(
    const XRInputSource* input_source);
device::mojom::blink::XRNativeOriginInformation Create(const XRPlane* plane);
device::mojom::blink::XRNativeOriginInformation Create(
    const XRLightProbe* light_probe);
device::mojom::blink::XRNativeOriginInformation Create(
    const XRReferenceSpace* reference_space);

device::mojom::blink::XRNativeOriginInformation Create(
    device::mojom::XRReferenceSpaceType reference_space_type);

}  // namespace XRNativeOriginInformation

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_NATIVE_ORIGIN_INFORMATION_H_
