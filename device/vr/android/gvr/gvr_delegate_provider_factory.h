// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_PROVIDER_FACTORY_H_
#define DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_PROVIDER_FACTORY_H_

#include <memory>

#include "device/vr/vr_export.h"

namespace device {

class GvrDelegateProvider;
class GvrDevice;

class DEVICE_VR_EXPORT GvrDelegateProviderFactory {
 public:
  static GvrDelegateProvider* Create();
  static void Install(std::unique_ptr<GvrDelegateProviderFactory> factory);
  static void SetDevice(GvrDevice* device) { device_ = device; }
  static GvrDevice* GetDevice() { return device_; }

  GvrDelegateProviderFactory(const GvrDelegateProviderFactory&) = delete;
  GvrDelegateProviderFactory& operator=(const GvrDelegateProviderFactory&) =
      delete;

  virtual ~GvrDelegateProviderFactory() = default;

 protected:
  GvrDelegateProviderFactory() = default;

  virtual GvrDelegateProvider* CreateGvrDelegateProvider() = 0;

  static GvrDevice* device_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_PROVIDER_FACTORY_H_
