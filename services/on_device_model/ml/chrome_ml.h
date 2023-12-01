// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace ml {

// A ChromeML object encapsulates a reference to the ChromeML library, exposing
// the library's API functions to callers and ensuring that the library remains
// loaded and usable throughout the object's lifetime.
class ChromeML {
 public:
  // Use Get() to acquire a global instance.
  ChromeML(base::PassKey<ChromeML>,
           base::ScopedNativeLibrary library,
           const ChromeMLAPI* api);
  ~ChromeML();

  // Gets a lazily initialized global instance of ChromeML. May return null
  // if the underlying library could not be loaded.
  static ChromeML* Get();

  // Exposes the raw ChromeMLAPI functions defined by the library.
  const ChromeMLAPI& api() const { return *api_; }

  // Whether or not the GPU is blocklisted.
  bool IsGpuBlocked() const;

 private:
  static std::unique_ptr<ChromeML> Create();

  const base::ScopedNativeLibrary library_;
  const raw_ptr<const ChromeMLAPI> api_;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_
