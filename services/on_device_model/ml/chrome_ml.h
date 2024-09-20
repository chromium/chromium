// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace ml {

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
base::FilePath GetChromeMLPath(
    const std::optional<std::string>& library_name = std::nullopt);

// A ChromeMLHolder object encapsulates a reference to the ChromeML shared
// library, exposing the library's API functions to callers and ensuring that
// the library remains loaded and usable throughout the object's lifetime.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) ChromeMLHolder {
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

class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) ChromeML {
 public:
  explicit ChromeML(const ChromeMLAPI* api);
  ~ChromeML();
  ChromeML(const ChromeML& other) = delete;
  ChromeML& operator=(const ChromeML& other) = delete;
  ChromeML(ChromeML&& other) = delete;
  ChromeML& operator=(ChromeML&& other) = delete;

  // Gets a lazily initialized global instance of ChromeML. May return null
  // if the underlying library could not be loaded.
  static ChromeML* Get(
      const std::optional<std::string>& library_name = std::nullopt);

  // Exposes the raw ChromeMLAPI functions defined by the library.
  const ChromeMLAPI& api() const { return *api_; }

 private:
  static std::unique_ptr<ChromeML> Create(
      const std::optional<std::string>& library_name);

  raw_ptr<const ChromeMLAPI> api_;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_
