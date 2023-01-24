// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/android/cardboard/cardboard_sdk.h"
#include "device/vr/vr_device_base.h"

namespace device {

class COMPONENT_EXPORT(VR_CARDBOARD) CardboardDevice : public VRDeviceBase {
 public:
  explicit CardboardDevice(std::unique_ptr<CardboardSdk> cardboard_sdk);

  CardboardDevice(const CardboardDevice&) = delete;
  CardboardDevice& operator=(const CardboardDevice&) = delete;

  ~CardboardDevice() override;

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void ShutdownSession(mojom::XRRuntime::ShutdownSessionCallback) override;

 private:
  std::unique_ptr<CardboardSdk> cardboard_sdk_;

  base::WeakPtrFactory<CardboardDevice> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_H_
