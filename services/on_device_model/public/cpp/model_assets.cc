// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_assets.h"

#include <cstdint>
#include <string_view>

#include "base/files/file.h"
#include "build/build_config.h"

namespace on_device_model {

namespace {

constexpr std::string_view kSpModelFile = "spm.model";
constexpr std::string_view kModelFile = "model.pb";
constexpr std::string_view kWeightsFile = "weights.bin";
constexpr std::string_view kTsDataFile = "ts.bin";
constexpr std::string_view kTsSpModelFile = "ts_spm.model";

}  // namespace

ModelAssets::ModelAssets() = default;

ModelAssets::ModelAssets(ModelAssets&&) = default;

ModelAssets& ModelAssets::operator=(ModelAssets&&) = default;

ModelAssets::~ModelAssets() = default;

ModelAssets LoadModelAssets(const base::FilePath& model_path,
                            const base::FilePath& ts_path) {
  ModelAssets assets;
  assets.sp_model = base::File(model_path.AppendASCII(kSpModelFile),
                               base::File::FLAG_OPEN | base::File::FLAG_READ);
  assets.model = base::File(model_path.AppendASCII(kModelFile),
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
  assets.ts_data = base::File(ts_path.AppendASCII(kTsDataFile),
                              base::File::FLAG_OPEN | base::File::FLAG_READ);
  assets.ts_sp_model =
      base::File(ts_path.AppendASCII(kTsSpModelFile),
                 base::File::FLAG_OPEN | base::File::FLAG_READ);

  // NOTE: Weights ultimately need to be mapped copy-on-write, but Fuchsia
  // (due to an apparent bug?) doesn't seem to support copy-on-write mapping of
  // file objects which are not writable. So we open as writable on Fuchsia even
  // though nothing should write through to the file.
#if BUILDFLAG(IS_FUCHSIA)
  constexpr uint32_t kWeightsFlags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE;
#else
  constexpr uint32_t kWeightsFlags =
      base::File::FLAG_OPEN | base::File::FLAG_READ;
#endif
  assets.weights =
      base::File(model_path.AppendASCII(kWeightsFile), kWeightsFlags);
  return assets;
}

}  // namespace on_device_model
