// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_assets.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

#if BUILDFLAG(IS_WIN)
#include "base/task/thread_pool.h"
#endif

namespace on_device_model {
namespace {

// Whether the on-device model should be loaded from a file path rather than a
// file descriptor. This may require disabling this service's sandbox.
//
// This flag is only for testing purposes and should NOT be enabled by default.
//
// Ideally this would be a FeatureParam of
// `optimization_guide::features::kOptimizationGuideOnDeviceModel` but including
// that header here results in a circular dependency which isn't worth
// unraveling for a flag which will never be used outside of local testing.
BASE_FEATURE(kForceLoadOnDeviceModelFromFilePathForTesting,
             "ForceLoadOnDeviceModelFromFilePathForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// NOTE: Weights ultimately need to be mapped copy-on-write, but Fuchsia
// (due to an apparent bug?) doesn't seem to support copy-on-write mapping of
// file objects which are not writable. So we open as writable on Fuchsia even
// though nothing should write through to the file.
#if BUILDFLAG(IS_FUCHSIA)
constexpr uint32_t kWeightsFlags =
    base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE;
constexpr uint32_t kCacheFlags = kWeightsFlags;
#else
constexpr uint32_t kWeightsFlags =
    base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC |
    base::File::FLAG_WIN_SEQUENTIAL_SCAN;
constexpr uint32_t kCacheFlags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_ASYNC |
                                 base::File::FLAG_WRITE;
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

ModelFile::ModelFile(base::File file) : file_(std::move(file)) {}

ModelFile::ModelFile(base::FilePath path) : file_(std::move(path)) {}

ModelFile::ModelFile(mojo::DefaultConstruct::Tag) {}

ModelFile::ModelFile(const ModelFile& other) {
  if (other.IsFile()) {
    file_ = other.file().Duplicate();
  } else {
    file_ = other.path();
  }
}

ModelFile& ModelFile::operator=(const ModelFile& other) {
  if (other.IsFile()) {
    file_ = other.file().Duplicate();
  } else {
    file_ = other.path();
  }
  return *this;
}

ModelFile::ModelFile(ModelFile&&) = default;
ModelFile& ModelFile::operator=(ModelFile&&) = default;
ModelFile::~ModelFile() = default;

base::File& ModelFile::file() {
  CHECK(std::holds_alternative<base::File>(file_));
  return std::get<base::File>(file_);
}

const base::File& ModelFile::file() const {
  CHECK(std::holds_alternative<base::File>(file_));
  return std::get<base::File>(file_);
}

const base::FilePath& ModelFile::path() const {
  CHECK(std::holds_alternative<base::FilePath>(file_));
  return std::get<base::FilePath>(file_);
}

bool ModelFile::IsFile() const {
  return std::holds_alternative<base::File>(file_);
}

// static
ModelAssets ModelAssets::FromFile(base::File file) {
  return ModelAssets(ModelFile(std::move(file)));
}

// static
ModelAssets ModelAssets::FromPath(base::FilePath path) {
  return ModelAssets(ModelFile(std::move(path)));
}

ModelAssets::ModelAssets(ModelFile weights) : weights(std::move(weights)) {}

ModelAssets::ModelAssets(mojo::DefaultConstruct::Tag tag) : weights(tag) {}

ModelAssets::ModelAssets(const ModelAssets& other)
    : weights(other.weights),
      sp_model_path(other.sp_model_path),
      cache(other.cache.Duplicate()) {}

ModelAssets& ModelAssets::operator=(const ModelAssets& other) {
  weights = other.weights;
  sp_model_path = other.sp_model_path;
  cache = other.cache.Duplicate();
  return *this;
}

ModelAssets::ModelAssets(ModelAssets&&) = default;
ModelAssets& ModelAssets::operator=(ModelAssets&&) = default;
ModelAssets::~ModelAssets() = default;

ModelAssets LoadModelAssets(const ModelAssetPaths& paths) {
  if (!paths.weights.empty()) {
    PrefetchFile(paths.weights);
  }

  auto assets =
      paths.weights.empty() ||
              base::FeatureList::IsEnabled(
                  kForceLoadOnDeviceModelFromFilePathForTesting)
          ? ModelAssets::FromPath(std::move(paths.weights))
          : ModelAssets::FromFile(base::File(paths.weights, kWeightsFlags));

  if (!paths.cache.empty()) {
    PrefetchFile(paths.cache);
    assets.cache = base::File(paths.cache, kCacheFlags);
  }

  return assets;
}

AdaptationAssetPaths::AdaptationAssetPaths() = default;
AdaptationAssetPaths::AdaptationAssetPaths(const AdaptationAssetPaths&) =
    default;
AdaptationAssetPaths::~AdaptationAssetPaths() = default;

AdaptationAssets::AdaptationAssets() = default;

AdaptationAssets::AdaptationAssets(const AdaptationAssets& other)
    : weights(other.weights.Duplicate()), weights_path(other.weights_path) {}

AdaptationAssets& AdaptationAssets::operator=(const AdaptationAssets& other) {
  weights = other.weights.Duplicate();
  weights_path = other.weights_path;
  return *this;
}

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
