// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_COMPACTION_TASK_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_COMPACTION_TASK_H_

#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"

namespace leveldb {
class DB;
}  // namespace leveldb

namespace content {

class IndexedDBCompactionTask
    : public IndexedDBPreCloseTaskQueue::PreCloseTask {
 public:
  explicit IndexedDBCompactionTask(leveldb::DB* database);
  ~IndexedDBCompactionTask() override;

  bool RequiresMetadata() const override;

  void Stop(IndexedDBPreCloseTaskQueue::StopReason reason) override;

  bool RunRound() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_COMPACTION_TASK_H_
