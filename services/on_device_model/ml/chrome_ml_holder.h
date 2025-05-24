// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_HOLDER_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_HOLDER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace ml {

base::FilePath GetChromeMLPath(
    const std::optional<std::string>& library_name = std::nullopt);

// A ChromeMLHolder object encapsulates a reference to the ChromeML shared
// library, exposing the library's API functions to callers and ensuring that
// the library remains loaded and usable throughout the object's lifetime.
class ChromeMLHolder {
 public:
  ChromeMLHolder(base::PassKey<ChromeMLHolder>,
                 base::ScopedNativeLibrary library,
                 const ChromeMLAPI* api);
  ~ChromeMLHolder();

  ChromeMLHolder(const ChromeMLHolder& other) = delete;
  ChromeMLHolder& operator=(const ChromeMLHolder& other) = delete;

  ChromeMLHolder(ChromeMLHolder&& other) = default;
  ChromeMLHolder& operator=(ChromeMLHolder&& other) = default;

  // Creates an instance of ChromeMLHolder. May return nullopt if the underlying
  // library could not be loaded.
  static std::unique_ptr<ChromeMLHolder> Create(
      const std::optional<std::string>& library_name = std::nullopt);

  // Exposes the raw ChromeMLAPI functions defined by the library.
  const ChromeMLAPI& api() const { return *api_; }

 private:
  base::ScopedNativeLibrary library_;
  raw_ptr<const ChromeMLAPI> api_;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_HOLDER_H_
