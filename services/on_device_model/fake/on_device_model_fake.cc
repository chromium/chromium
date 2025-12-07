// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/fake/on_device_model_fake.h"

#include "base/no_destructor.h"
#include "services/on_device_model/fake/fake_chrome_ml_api.h"
#include "services/on_device_model/ml/chrome_ml.h"

namespace fake_ml {

COMPONENT_EXPORT(ON_DEVICE_MODEL_FAKE)
const ml::ChromeML* GetFakeChromeML() {
  static const base::NoDestructor<std::unique_ptr<ml::ChromeML>> fake{
      ml::ChromeML::CreateForTesting(fake_ml::GetFakeMlApi())};
  return fake->get();
}

}  // namespace fake_ml
