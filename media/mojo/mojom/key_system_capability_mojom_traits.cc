// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/key_system_capability_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::KeySystemCapabilityDataView,
                  media::KeySystemCapability>::
    Read(media::mojom::KeySystemCapabilityDataView input,
         media::KeySystemCapability* output) {
  std::optional<media::CdmCapability> sw_secure_capability;
  if (!input.ReadSwSecureCapability(&sw_secure_capability)) {
    return false;
  }

  std::optional<media::CdmCapability> hw_secure_capability;
  if (!input.ReadHwSecureCapability(&hw_secure_capability)) {
    return false;
  }

  *output = media::KeySystemCapability(std::move(sw_secure_capability),
                                       std::move(hw_secure_capability));
  return true;
}

}  // namespace mojo
