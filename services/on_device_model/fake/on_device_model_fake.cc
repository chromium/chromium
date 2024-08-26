// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/fake/fake_chrome_ml_api.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/on_device_model_internal.h"

namespace fake_ml {

namespace {

const ml::ChromeML* GetFakeChromeML() {
  static const base::NoDestructor<ml::ChromeML> fake{fake_ml::GetFakeMlApi()};
  return fake.get();
}

}  // namespace

COMPONENT_EXPORT(ON_DEVICE_MODEL_FAKE)
const ml::OnDeviceModelInternalImpl* GetOnDeviceModelFakeImpl() {
  static const base::NoDestructor<ml::OnDeviceModelInternalImpl> impl(
      GetFakeChromeML(), ml::GpuBlocklist{.skip_for_testing = true});
  return impl.get();
}

}  // namespace on_device_model
