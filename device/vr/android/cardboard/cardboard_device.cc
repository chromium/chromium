// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_device.h"

#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/notreached.h"

namespace device {

namespace {

const std::vector<mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<mojom::XRSessionFeature>>
      kSupportedFeatures{{
          mojom::XRSessionFeature::REF_SPACE_VIEWER,
          mojom::XRSessionFeature::REF_SPACE_LOCAL,
          mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
      }};

  return *kSupportedFeatures;
}

}  // namespace

CardboardDevice::CardboardDevice(std::unique_ptr<CardboardSdk> cardboard_sdk)
    : VRDeviceBase(mojom::XRDeviceId::CARDBOARD_DEVICE_ID),
      cardboard_sdk_(std::move(cardboard_sdk)) {
  SetSupportedFeatures(GetSupportedFeatures());
}

CardboardDevice::~CardboardDevice() = default;

void CardboardDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  // TODO(https://crbug.com/989117): Implement
  std::move(callback).Run(nullptr);
}

void CardboardDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  NOTREACHED();
}

}  // namespace device
