// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_COMPACTION_TASK_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_COMPACTION_TASK_H_

#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"

namespace leveldb {
class DB;
}  // namespace leveldb

namespace content::indexed_db {

class IndexedDBCompactionTask
    : public BackingStorePreCloseTaskQueue::PreCloseTask {
 public:
  explicit IndexedDBCompactionTask(leveldb::DB* database);
  ~IndexedDBCompactionTask() override;

  bool RequiresMetadata() const override;

  bool RunRound() override;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_COMPACTION_TASK_H_
