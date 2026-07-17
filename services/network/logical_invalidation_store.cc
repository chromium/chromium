// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/logical_invalidation_store.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
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

base::TimeDelta SerializeAndWriteInvalidationFiltersFile(
    const base::FilePath& path,
    const LogicalInvalidationStore::InvalidationFilterVector& filters) {
  base::ElapsedTimer timer;
  base::Pickle pickle;
  net::WriteToPickle(pickle, filters);
  base::span<const uint8_t> bytes = pickle.AsBytes();
  base::ImportantFileWriter::WriteFileAtomically(path,
                                                 base::as_string_view(bytes));
  return timer.Elapsed();
}

std::pair<LogicalInvalidationStore::LoadResult,
          LogicalInvalidationStore::InvalidationFilterVector>
LoadInvalidationFiltersFile(const base::FilePath& path) {
  std::optional<std::vector<uint8_t>> data = base::ReadFileToBytes(path);
  if (!data) {
    if (!base::PathExists(path)) {
      return {LogicalInvalidationStore::LoadResult::kFileNotFound, {}};
    }
    return {LogicalInvalidationStore::LoadResult::kCorrupt, {}};
  }
  base::Pickle pickle = base::Pickle::WithData(base::span(*data));
  base::PickleIterator iter(pickle);
  auto maybe_filters = net::ReadValueFromPickle<
      LogicalInvalidationStore::InvalidationFilterVector>(iter);
  if (!maybe_filters || !iter.ReachedEnd()) {
    return {LogicalInvalidationStore::LoadResult::kCorrupt, {}};
  }
  for (auto& filter : *maybe_filters) {
    filter.was_loaded_from_disk = true;
  }
  return {LogicalInvalidationStore::LoadResult::kSuccess,
          std::move(*maybe_filters)};
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
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void LogicalInvalidationStore::OnLoaded(
    base::TimeTicks load_start_time,
    LoadCallback callback,
    std::pair<LoadResult, InvalidationFilterVector> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramTimes(
      "Net.HttpCache.LogicalInvalidation.PersistenceLoadDuration",
      base::TimeTicks::Now() - load_start_time);
  base::UmaHistogramEnumeration("Net.HttpCache.LogicalInvalidation.LoadResult",
                                result.first);
  if (result.first == LoadResult::kSuccess) {
    base::UmaHistogramCounts100(
        "Net.HttpCache.LogicalInvalidation.LoadedFilterCount",
        result.second.size());
  }
  std::move(callback).Run(result.first, std::move(result.second));
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

void LogicalInvalidationStore::OnSaved(base::OnceClosure callback,
                                       base::TimeDelta write_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramTimes(
      "Net.HttpCache.LogicalInvalidation.PersistenceWriteDuration",
      write_duration);
  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace network
