// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_LOGICAL_INVALIDATION_STORE_H_
#define SERVICES_NETWORK_LOGICAL_INVALIDATION_STORE_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "net/http/http_cache.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) LogicalInvalidationStore {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(LoadResult)
  enum class LoadResult {
    kSuccess = 0,
    kFileNotFound = 1,
    kCorrupt = 2,
    kMaxValue = kCorrupt
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:LogicalInvalidationStoreLoadResult)

  using InvalidationFilterVector =
      std::vector<net::HttpCache::InvalidationFilter>;

  using LoadCallback =
      base::OnceCallback<void(LoadResult, InvalidationFilterVector)>;

  LogicalInvalidationStore(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~LogicalInvalidationStore();

  LogicalInvalidationStore(const LogicalInvalidationStore&) = delete;
  LogicalInvalidationStore& operator=(const LogicalInvalidationStore&) = delete;

  void Load(LoadCallback callback);
  void Save(const InvalidationFilterVector& filters,
            base::OnceClosure callback = base::OnceClosure());

 private:
  void OnLoaded(base::TimeTicks load_start_time,
                LoadCallback callback,
                std::pair<LoadResult, InvalidationFilterVector> result);
  void OnSaved(base::OnceClosure callback, base::TimeDelta write_duration);

  base::FilePath file_path_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LogicalInvalidationStore> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_LOGICAL_INVALIDATION_STORE_H_
