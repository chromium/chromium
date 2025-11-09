// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/text_safety_assets.h"

#include <cstdint>
#include <string_view>

#include "base/files/file.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
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
    if (params.ts_paths->sp_model.empty()) {
      auto bs_assets = mojom::BertSafetyModelAssets::New();
      bs_assets->model = base::File(
          params.ts_paths->data, base::File::FLAG_OPEN | base::File::FLAG_READ);
      auto bs_assets_union =
          mojom::SafetyModelAssets::NewBsAssets(std::move(bs_assets));
      result->safety_assets = std::move(bs_assets_union);
    } else {
      auto ts_assets = mojom::TextSafetyModelAssets::New();
      ts_assets->data = base::File(
          params.ts_paths->data, base::File::FLAG_OPEN | base::File::FLAG_READ);
      ts_assets->sp_model =
          base::File(params.ts_paths->sp_model,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
      auto ts_assets_union =
          mojom::SafetyModelAssets::NewTsAssets(std::move(ts_assets));
      result->safety_assets = std::move(ts_assets_union);
    }
  }
  return result;
}

}  // namespace on_device_model
