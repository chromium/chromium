// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_assets.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"

#if BUILDFLAG(IS_WIN)
#include "base/task/thread_pool.h"
#endif

namespace on_device_model {
namespace {

// NOTE: Weights ultimately need to be mapped copy-on-write, but Fuchsia
// (due to an apparent bug?) doesn't seem to support copy-on-write mapping of
// file objects which are not writable. So we open as writable on Fuchsia even
// though nothing should write through to the file.
#if BUILDFLAG(IS_FUCHSIA)
constexpr uint32_t kWeightsFlags =
    base::File::AddFlagsForPassingToUntrustedProcess(
        base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
constexpr uint32_t kCacheFlags = kWeightsFlags;
#else
constexpr uint32_t kWeightsFlags =
    base::File::AddFlagsForPassingToUntrustedProcess(
        base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC |
        base::File::FLAG_WIN_SEQUENTIAL_SCAN);
constexpr uint32_t kCacheFlags =
    base::File::AddFlagsForPassingToUntrustedProcess(
        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
        base::File::FLAG_WRITE);
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

// Extension of the file stored next to each cache recording the fingerprint of
// the configuration that last wrote the cache.
constexpr base::FilePath::CharType kCacheFingerprintExtension[] =
    FILE_PATH_LITERAL(".fingerprint");

// Legacy extension of the program cache fingerprint file, which only ever
// stored the Dawn version.
// TODO(crbug.com/585215282): Remove the migration once most clients have
// loaded a GPU model with the new file name.
constexpr base::FilePath::CharType kLegacyDawnVersionExtension[] =
    FILE_PATH_LITERAL(".dawn_version");

// Opens the cache at `path`. If the fingerprint stored alongside the cache is
// missing or differs from `fingerprint`, the cache is truncated so the backend
// rebuilds it from scratch, and `fingerprint` is stored for the next load.
// Returns an invalid file if the cache can't be invalidated.
base::File OpenAndValidateCache(const base::FilePath& path,
                                std::string_view fingerprint) {
  DCHECK(!path.empty());

  base::File cache_file(path, kCacheFlags);
  if (!cache_file.IsValid()) {
    return cache_file;
  }

  base::FilePath fingerprint_path = GetCacheFingerprintPath(path);
  std::string cached_fingerprint;
  bool fingerprint_match =
      base::ReadFileToString(fingerprint_path, &cached_fingerprint) &&
      cached_fingerprint == fingerprint;

  if (fingerprint_match) {
    PrefetchFile(path);
    return cache_file;
  }

  // First run or the cache was written by a different configuration.
  // Invalidate by truncating the file to 0. If either step fails, drop the
  // fingerprint and skip the cache so a stale cache is never marked valid.
  if (!cache_file.SetLength(0) ||
      !base::WriteFile(fingerprint_path, fingerprint)) {
    base::DeleteFile(fingerprint_path);
    return base::File();
  }

  return cache_file;
}

// Opens a weight cache, validating it against `fingerprint` if non-empty.
base::File OpenWeightCache(const base::FilePath& path,
                           std::string_view fingerprint) {
  if (path.empty()) {
    return base::File();
  }
  if (!fingerprint.empty()) {
    return OpenAndValidateCache(path, fingerprint);
  }
  PrefetchFile(path);
  return base::File(path, kCacheFlags);
}

// Opens a program cache, validating it against the current Dawn version.
base::File OpenAndValidateProgramCache(const base::FilePath& path) {
  // dawnProcGetVersion() returns a static array of uint8_t of size 20.
  constexpr size_t kDawnVersionSize = 20;

  if (path.empty()) {
    return base::File();
  }

  // Move the legacy Dawn version file to the new fingerprint file so existing
  // program caches are not invalidated by the rename.
  base::FilePath legacy_path = path.AddExtension(kLegacyDawnVersionExtension);
  if (base::PathExists(legacy_path)) {
    base::Move(legacy_path, GetCacheFingerprintPath(path));
  }

  return OpenAndValidateCache(
      path,
      std::string_view(reinterpret_cast<const char*>(dawnProcGetVersion()),
                       kDawnVersionSize));
}

}  // namespace

base::FilePath GetCacheFingerprintPath(const base::FilePath& cache_path) {
  return cache_path.AddExtension(kCacheFingerprintExtension);
}

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
ModelFile::~ModelFile() {
  if (IsFile() && file().IsValid()) {
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(std::move(file())));
  }
}

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
      cache(other.cache.Duplicate()),
      encoder_cache(other.encoder_cache.Duplicate()),
      adapter_cache(other.adapter_cache.Duplicate()),
      program_cache(other.program_cache.Duplicate()),
      encoder_program_cache(other.encoder_program_cache.Duplicate()),
      adapter_program_cache(other.adapter_program_cache.Duplicate()),
      mtp_drafter_cache(other.mtp_drafter_cache.Duplicate()),
      mtp_drafter_program_cache(other.mtp_drafter_program_cache.Duplicate()) {}

ModelAssets& ModelAssets::operator=(const ModelAssets& other) {
  weights = other.weights;
  sp_model_path = other.sp_model_path;
  cache = other.cache.Duplicate();
  encoder_cache = other.encoder_cache.Duplicate();
  adapter_cache = other.adapter_cache.Duplicate();
  program_cache = other.program_cache.Duplicate();
  encoder_program_cache = other.encoder_program_cache.Duplicate();
  adapter_program_cache = other.adapter_program_cache.Duplicate();
  mtp_drafter_cache = other.mtp_drafter_cache.Duplicate();
  mtp_drafter_program_cache = other.mtp_drafter_program_cache.Duplicate();
  return *this;
}

ModelAssets::ModelAssets(ModelAssets&&) = default;
ModelAssets& ModelAssets::operator=(ModelAssets&&) = default;
ModelAssets::~ModelAssets() = default;

ModelAssets LoadModelAssets(const ModelAssetPaths& paths) {
  TRACE_EVENT("optimization_guide", "LoadModelAssets");
  if (!paths.weights.empty()) {
    PrefetchFile(paths.weights);
  }

  auto assets =
      paths.weights.empty()
          ? ModelAssets::FromPath(std::move(paths.weights))
          : ModelAssets::FromFile(base::File(paths.weights, kWeightsFlags));

  // Weight caches are validated against the configuration that uses them, and
  // program caches against the Dawn version.
  const std::string& weight_fingerprint = paths.weight_cache_fingerprint;
  assets.cache = OpenWeightCache(paths.cache, weight_fingerprint);
  assets.encoder_cache =
      OpenWeightCache(paths.encoder_cache, weight_fingerprint);
  assets.adapter_cache =
      OpenWeightCache(paths.adapter_cache, weight_fingerprint);
  assets.mtp_drafter_cache =
      OpenWeightCache(paths.mtp_drafter_cache, weight_fingerprint);

  assets.program_cache = OpenAndValidateProgramCache(paths.program_cache);
  assets.encoder_program_cache =
      OpenAndValidateProgramCache(paths.encoder_program_cache);
  assets.adapter_program_cache =
      OpenAndValidateProgramCache(paths.adapter_program_cache);
  assets.mtp_drafter_program_cache =
      OpenAndValidateProgramCache(paths.mtp_drafter_program_cache);

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
  TRACE_EVENT("optimization_guide", "LoadAdaptationAssets");
  AdaptationAssets assets;
  if (!paths.weights.empty()) {
    PrefetchFile(paths.weights);
    assets.weights = base::File(paths.weights, kWeightsFlags);
  }
  return assets;
}

}  // namespace on_device_model
