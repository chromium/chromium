// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/leveldatabase/env_chromium.h"

using blink::IndexedDBDatabaseMetadata;

namespace content::indexed_db {

BackingStorePreCloseTaskQueue::PreCloseTask::PreCloseTask(leveldb::DB* database)
    : database_(database) {}

BackingStorePreCloseTaskQueue::PreCloseTask::~PreCloseTask() = default;

bool BackingStorePreCloseTaskQueue::PreCloseTask::RequiresMetadata() const {
  return false;
}

void BackingStorePreCloseTaskQueue::PreCloseTask::SetMetadata(
    const std::vector<blink::IndexedDBDatabaseMetadata>* metadata) {}

BackingStorePreCloseTaskQueue::BackingStorePreCloseTaskQueue(
    std::list<std::unique_ptr<BackingStorePreCloseTaskQueue::PreCloseTask>>
        tasks,
    base::OnceClosure on_complete,
    base::TimeDelta max_run_time,
    std::unique_ptr<base::OneShotTimer> timer)
    : tasks_(std::move(tasks)),
      on_done_(std::move(on_complete)),
      timeout_time_(max_run_time),
      timeout_timer_(std::move(timer)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

BackingStorePreCloseTaskQueue::~BackingStorePreCloseTaskQueue() = default;

void BackingStorePreCloseTaskQueue::Stop() {
  if (!started_ || done_) {
    return;
  }
  DCHECK(!tasks_.empty());
  while (!tasks_.empty()) {
    tasks_.pop_front();
  }
  OnComplete();
}

void BackingStorePreCloseTaskQueue::Start(MetadataFetcher metadata_fetcher) {
  DCHECK(!started_);
  started_ = true;
  if (tasks_.empty()) {
    OnComplete();
    return;
  }
  timeout_timer_->Start(
      FROM_HERE, timeout_time_,
      base::BindOnce(&BackingStorePreCloseTaskQueue::StopForTimout,
                     ptr_factory_.GetWeakPtr()));
  metadata_fetcher_ = std::move(metadata_fetcher);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&BackingStorePreCloseTaskQueue::RunLoop,
                                        ptr_factory_.GetWeakPtr()));
}

void BackingStorePreCloseTaskQueue::OnComplete() {
  DCHECK(started_);
  DCHECK(!done_);
  ptr_factory_.InvalidateWeakPtrs();
  timeout_timer_->Stop();
  done_ = true;
  std::move(on_done_).Run();
}

void BackingStorePreCloseTaskQueue::StopForTimout() {
  DCHECK(started_);
  if (done_) {
    return;
  }
  while (!tasks_.empty()) {
    tasks_.pop_front();
  }
  OnComplete();
}

void BackingStorePreCloseTaskQueue::StopForMetadataError(const Status& status) {
  if (done_) {
    return;
  }

  LOCAL_HISTOGRAM_ENUMERATION(
      "WebCore.IndexedDB.BackingStorePreCloseTaskList.MetadataError",
      leveldb_env::GetLevelDBStatusUMAValue(status.leveldb_status()),
      leveldb_env::LEVELDB_STATUS_MAX);
  while (!tasks_.empty()) {
    tasks_.pop_front();
  }
  OnComplete();
}

void BackingStorePreCloseTaskQueue::RunLoop() {
  if (done_) {
    return;
  }

  if (tasks_.empty()) {
    OnComplete();
    return;
  }

  PreCloseTask* task = tasks_.front().get();
  if (task->RequiresMetadata() && !task->set_metadata_was_called_) {
    if (!has_metadata_) {
      Status status = std::move(metadata_fetcher_).Run(&metadata_);
      has_metadata_ = true;
      if (!status.ok()) {
        StopForMetadataError(status);
        return;
      }
    }
    task->SetMetadata(&metadata_);
    task->set_metadata_was_called_ = true;
  }
  bool done = task->RunRound();
  if (done) {
    tasks_.pop_front();
    if (tasks_.empty()) {
      OnComplete();
      return;
    }
  }
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&BackingStorePreCloseTaskQueue::RunLoop,
                                        ptr_factory_.GetWeakPtr()));
}

}  // namespace content::indexed_db
