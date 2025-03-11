// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CAPABILITIES_MOJOM_TRAITS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CAPABILITIES_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
    StructTraits<on_device_model::mojom::CapabilitiesDataView,
                 on_device_model::Capabilities> {
  static bool image_input(const on_device_model::Capabilities& capabilities) {
    return capabilities.Has(on_device_model::CapabilityFlags::kImageInput);
  }

  static bool audio_input(const on_device_model::Capabilities& capabilities) {
    return capabilities.Has(on_device_model::CapabilityFlags::kAudioInput);
  }

  static bool Read(on_device_model::mojom::CapabilitiesDataView data,
                   on_device_model::Capabilities* out) {
    out->Clear();

    if (data.image_input()) {
      out->Put(on_device_model::CapabilityFlags::kImageInput);
    }
    if (data.audio_input()) {
      out->Put(on_device_model::CapabilityFlags::kAudioInput);
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CAPABILITIES_MOJOM_TRAITS_H_
