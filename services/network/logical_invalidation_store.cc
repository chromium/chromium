// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/logical_invalidation_store.h"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/pickle.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/pickle.h"
#include "net/base/pickle_traits.h"
#include "net/http/http_cache_invalidation_pickle_traits.h"

namespace network {

namespace {

constexpr char kInvalidationFiltersFileName[] = "invalidation_filters";

// Returns whether the write succeeded, and how long the attempt took.
std::pair<bool, base::TimeDelta> SerializeAndWriteInvalidationFiltersFile(
    const base::FilePath& path,
    const LogicalInvalidationStore::InvalidationFilterVector& filters) {
  base::ElapsedTimer timer;
  // WriteFileAtomically() does not create the parent directory, and no
  // embedder reliably does: Android bypasses the sandbox setup that creates it
  // elsewhere. Create it on every save; base::CreateDirectory() succeeds if the
  // directory already exists, costing one stat() in the steady state, and
  // tolerates racing the browser process. It only fails on a path that cannot
  // be a directory at all, where the write would fail anyway.
  if (!base::CreateDirectory(path.DirName())) {
    return {false, timer.Elapsed()};
  }
  base::Pickle pickle;
  net::WriteToPickle(pickle, filters);
  base::span<const uint8_t> bytes = pickle.AsBytes();
  const bool success = base::ImportantFileWriter::WriteFileAtomically(
      path, base::as_string_view(bytes));
  return {success, timer.Elapsed()};
}

// Returns the load result, loaded filters on success, and how long the attempt
// took.
std::tuple<LogicalInvalidationStore::LoadResult,
           LogicalInvalidationStore::InvalidationFilterVector,
           base::TimeDelta>
LoadInvalidationFiltersFile(const base::FilePath& path) {
  base::ElapsedTimer timer;
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    if (file.error_details() == base::File::FILE_ERROR_NOT_FOUND) {
      return {LogicalInvalidationStore::LoadResult::kFileNotFound,
              {},
              timer.Elapsed()};
    }
    return {
        LogicalInvalidationStore::LoadResult::kCorrupt, {}, timer.Elapsed()};
  }
  base::ScopedFILE stream(base::FileToFILE(std::move(file), "rb"));
  std::string data;
  if (!stream || !base::ReadStreamToString(stream.get(), &data)) {
    return {
        LogicalInvalidationStore::LoadResult::kCorrupt, {}, timer.Elapsed()};
  }
  base::Pickle pickle = base::Pickle::WithData(base::as_byte_span(data));
  base::PickleIterator iter(pickle);
  auto maybe_filters = net::ReadValueFromPickle<
      LogicalInvalidationStore::InvalidationFilterVector>(iter);
  if (!maybe_filters || !iter.ReachedEnd()) {
    return {
        LogicalInvalidationStore::LoadResult::kCorrupt, {}, timer.Elapsed()};
  }
  for (auto& filter : *maybe_filters) {
    filter.was_loaded_from_disk = true;
  }
  return {LogicalInvalidationStore::LoadResult::kSuccess,
          std::move(*maybe_filters), timer.Elapsed()};
}

}  // namespace

LogicalInvalidationStore::LogicalInvalidationStore(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : file_path_(path.AppendASCII(kInvalidationFiltersFileName)),
      file_task_runner_(file_task_runner) {
  CHECK(!file_path_.empty());
  CHECK(file_task_runner_);
}

LogicalInvalidationStore::~LogicalInvalidationStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LogicalInvalidationStore::Load(LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadInvalidationFiltersFile, file_path_),
      base::BindOnce(&LogicalInvalidationStore::OnLoaded,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LogicalInvalidationStore::OnLoaded(LoadCallback callback,
                                        LoadResult load_result,
                                        InvalidationFilterVector filters,
                                        base::TimeDelta load_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramEnumeration("Net.HttpCache.LogicalInvalidation.LoadResult",
                                load_result);
  // Only time successful loads. Missing or corrupt files bail out before
  // reading or deserializing completes, so their duration would skew the
  // distribution.
  if (load_result == LoadResult::kSuccess) {
    base::UmaHistogramTimes(
        "Net.HttpCache.LogicalInvalidation.PersistenceLoadDuration",
        load_duration);
    base::UmaHistogramCounts100(
        "Net.HttpCache.LogicalInvalidation.LoadedFilterCount", filters.size());
  }
  std::move(callback).Run(load_result, std::move(filters));
}

void LogicalInvalidationStore::Save(const InvalidationFilterVector& filters,
                                    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SerializeAndWriteInvalidationFiltersFile, file_path_,
                     filters),
      base::BindOnce(&LogicalInvalidationStore::OnSaved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LogicalInvalidationStore::OnSaved(
    base::OnceClosure callback,
    std::pair<bool, base::TimeDelta> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto [success, write_duration] = result;
  base::UmaHistogramBoolean(
      "Net.HttpCache.LogicalInvalidation.PersistenceWriteSuccess", success);
  // Only time successful writes. A failure can bail out before the write even
  // starts, so its near-zero duration would skew the distribution.
  if (success) {
    base::UmaHistogramTimes(
        "Net.HttpCache.LogicalInvalidation.PersistenceWriteDuration",
        write_duration);
  }
  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace network
