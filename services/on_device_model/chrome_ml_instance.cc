// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/chrome_ml_instance.h"

#include <memory>

#include "base/no_destructor.h"
#include "third_party/ml/public/chrome_ml.h"

namespace on_device_model {

ml::ChromeML* GetChromeMLInstance() {
  static base::NoDestructor<std::unique_ptr<ml::ChromeML>> ml{
      ml::ChromeML::Create()};
  return ml->get();
}

}  // namespace on_device_model
