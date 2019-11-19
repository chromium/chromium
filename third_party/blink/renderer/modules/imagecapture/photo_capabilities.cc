// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/photo_capabilities.h"

namespace blink {

Vector<String> PhotoCapabilities::fillLightMode() const {
  Vector<String> fill_light_modes;
  for (const auto& mode : fill_light_modes_) {
    switch (mode) {
      case media::mojom::blink::FillLightMode::OFF:
        fill_light_modes.push_back("off");
        break;
      case media::mojom::blink::FillLightMode::AUTO:
        fill_light_modes.push_back("auto");
        break;
      case media::mojom::blink::FillLightMode::FLASH:
        fill_light_modes.push_back("flash");
        break;
      default:
        NOTREACHED();
    }
  }
  return fill_light_modes;
}

String PhotoCapabilities::redEyeReduction() const {
  switch (red_eye_reduction_) {
    case media::mojom::blink::RedEyeReduction::NEVER:
      return "never";
    case media::mojom::blink::RedEyeReduction::ALWAYS:
      return "always";
    case media::mojom::blink::RedEyeReduction::CONTROLLABLE:
      return "controllable";
    default:
      NOTREACHED();
  }
  return "";
}

bool PhotoCapabilities::IsRedEyeReductionControllable() const {
  return red_eye_reduction_ ==
         media::mojom::blink::RedEyeReduction::CONTROLLABLE;
}

void PhotoCapabilities::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_height_);
  visitor->Trace(image_width_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
