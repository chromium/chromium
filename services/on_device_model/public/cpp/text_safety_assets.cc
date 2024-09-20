// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/text_safety_assets.h"

#include <cstdint>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-forward.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

TextSafetyAssetPaths::TextSafetyAssetPaths() = default;
TextSafetyAssetPaths::TextSafetyAssetPaths(const TextSafetyAssetPaths&) =
    default;
TextSafetyAssetPaths::~TextSafetyAssetPaths() = default;

LanguageDetectionAssetPaths::LanguageDetectionAssetPaths() = default;
LanguageDetectionAssetPaths::LanguageDetectionAssetPaths(
    const LanguageDetectionAssetPaths&) = default;
LanguageDetectionAssetPaths::~LanguageDetectionAssetPaths() = default;

TextSafetyLoaderParams::TextSafetyLoaderParams() = default;
TextSafetyLoaderParams::TextSafetyLoaderParams(const TextSafetyLoaderParams&) =
    default;
TextSafetyLoaderParams::~TextSafetyLoaderParams() = default;

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
mojom::TextSafetyModelParamsPtr LoadTextSafetyParams(
    TextSafetyLoaderParams params) {
  auto result = mojom::TextSafetyModelParams::New();
  if (params.language_paths) {
    result->language_assets = mojom::LanguageModelAssets::New();
    result->language_assets->model =
        base::File(params.language_paths->model,
                   base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
  if (params.ts_paths) {
    result->ts_assets = mojom::TextSafetyModelAssets::New();
    result->ts_assets->data = base::File(
        params.ts_paths->data, base::File::FLAG_OPEN | base::File::FLAG_READ);
    result->ts_assets->sp_model =
        base::File(params.ts_paths->sp_model,
                   base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
  return result;
}

}  // namespace on_device_model
