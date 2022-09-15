// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_compaction_task.h"

#include "base/trace_event/base_tracing.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace content {

IndexedDBCompactionTask::IndexedDBCompactionTask(leveldb::DB* database)
    : IndexedDBPreCloseTaskQueue::PreCloseTask(database) {}

IndexedDBCompactionTask::~IndexedDBCompactionTask() = default;

bool IndexedDBCompactionTask::RequiresMetadata() const {
  return false;
}

void IndexedDBCompactionTask::Stop(
    IndexedDBPreCloseTaskQueue::StopReason reason) {}

bool IndexedDBCompactionTask::RunRound() {
  TRACE_EVENT0("IndexedDB", "CompactRange");
  database()->CompactRange(nullptr, nullptr);
  return true;
}

}  // namespace content
