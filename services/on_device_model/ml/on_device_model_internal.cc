// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/on_device_model_internal.h"

#include <memory>

#include "base/no_destructor.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/gpu_blocklist.h"

namespace ml {

OnDeviceModelInternalImpl::OnDeviceModelInternalImpl(const ChromeML* chrome_ml)
    : chrome_ml_(chrome_ml) {}

OnDeviceModelInternalImpl::~OnDeviceModelInternalImpl() = default;

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const OnDeviceModelInternalImpl* GetOnDeviceModelInternalImpl() {
  static const base::NoDestructor<OnDeviceModelInternalImpl> impl(
      ::ml::ChromeML::Get());
  return impl.get();
}

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const OnDeviceModelInternalImpl*
GetOnDeviceModelInternalImplWithoutGpuBlocklistForTesting() {
  return GetOnDeviceModelInternalImpl();
}

}  // namespace ml
