// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEXT_SAFETY_ASSETS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEXT_SAFETY_ASSETS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

// A bundle of file paths to use for loading an adaptation.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) TextSafetyAssetPaths {
  TextSafetyAssetPaths();
  TextSafetyAssetPaths(const TextSafetyAssetPaths&);
  ~TextSafetyAssetPaths();

  base::FilePath data;
  base::FilePath sp_model;
};

// A bundle of file paths to use for loading an adaptation.
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) LanguageDetectionAssetPaths {
  LanguageDetectionAssetPaths();
  LanguageDetectionAssetPaths(const LanguageDetectionAssetPaths&);
  ~LanguageDetectionAssetPaths();

  base::FilePath model;
};

struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) TextSafetyLoaderParams {
  TextSafetyLoaderParams();
  TextSafetyLoaderParams(const TextSafetyLoaderParams&);
  ~TextSafetyLoaderParams();

  std::optional<TextSafetyAssetPaths> ts_paths;
  std::optional<LanguageDetectionAssetPaths> language_paths;
};

// Load assets for text safety model.
COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
mojom::TextSafetyModelParamsPtr LoadTextSafetyParams(
    TextSafetyLoaderParams params);

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEXT_SAFETY_ASSETS_H_
