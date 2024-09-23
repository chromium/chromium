// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_assets.h"

#include <cstdint>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"

namespace on_device_model {
namespace {

// NOTE: Weights ultimately need to be mapped copy-on-write, but Fuchsia
// (due to an apparent bug?) doesn't seem to support copy-on-write mapping of
// file objects which are not writable. So we open as writable on Fuchsia even
// though nothing should write through to the file.
#if BUILDFLAG(IS_FUCHSIA)
constexpr uint32_t kWeightsFlags =
    base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE;
#else
constexpr uint32_t kWeightsFlags =
    base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC |
    base::File::FLAG_WIN_SEQUENTIAL_SCAN;
#endif

// Attempts to make sure `file` will be read from disk quickly when needed.
void PrefetchFile(const base::FilePath& path) {
  constexpr bool kIsExecutable = false;
  constexpr bool kSequential = true;
#if BUILDFLAG(IS_WIN)
  // On Windows PreReadFile() can take on the order of hundreds of milliseconds,
  // so run on a separate thread.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& path) {
            base::PreReadFile(path, kIsExecutable, kSequential);
          },
          path));
#else
  base::PreReadFile(path, kIsExecutable, kSequential);
#endif
}

}  // namespace

ModelAssetPaths::ModelAssetPaths() = default;
ModelAssetPaths::ModelAssetPaths(const ModelAssetPaths&) = default;
ModelAssetPaths::~ModelAssetPaths() = default;

ModelAssets::ModelAssets() = default;
ModelAssets::ModelAssets(ModelAssets&&) = default;
ModelAssets& ModelAssets::operator=(ModelAssets&&) = default;
ModelAssets::~ModelAssets() = default;

ModelAssets LoadModelAssets(const ModelAssetPaths& paths) {
  if (!paths.weights.empty()) {
    PrefetchFile(paths.weights);
  }

  ModelAssets assets;
  if (!paths.weights.empty()) {
    assets.weights = base::File(paths.weights, kWeightsFlags);
  }
  return assets;
}

AdaptationAssetPaths::AdaptationAssetPaths() = default;
AdaptationAssetPaths::AdaptationAssetPaths(const AdaptationAssetPaths&) =
    default;
AdaptationAssetPaths::~AdaptationAssetPaths() = default;

AdaptationAssets::AdaptationAssets() = default;
AdaptationAssets::AdaptationAssets(AdaptationAssets&&) = default;
AdaptationAssets& AdaptationAssets::operator=(AdaptationAssets&&) = default;
AdaptationAssets::~AdaptationAssets() = default;

AdaptationAssets LoadAdaptationAssets(const AdaptationAssetPaths& paths) {
  AdaptationAssets assets;
  if (!paths.weights.empty()) {
    PrefetchFile(paths.weights);
    assets.weights = base::File(paths.weights, kWeightsFlags);
  }
  return assets;
}

}  // namespace on_device_model
