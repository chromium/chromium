// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CAPABILITIES_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CAPABILITIES_H_

#include "base/containers/enum_set.h"

namespace on_device_model {

// A set of capabilities a model can have.
enum class CapabilityFlags {
  // Model supports image input.
  kImageInput,
  // Model supports audio input.
  kAudioInput,

  kMinValue = kImageInput,
  kMaxValue = kAudioInput,
};
using Capabilities = base::EnumSet<CapabilityFlags,
                                   CapabilityFlags::kMinValue,
                                   CapabilityFlags::kMaxValue>;

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CAPABILITIES_H_
