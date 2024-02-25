// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/initialization.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "sql/vfs_wrapper.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

void EnsureSqliteInitialized(bool create_wrapper) {
  // sqlite3_initialize() uses double-checked locking and thus can have
  // data races.
  static base::NoDestructor<base::Lock> sqlite_init_lock;
  base::AutoLock auto_lock(*sqlite_init_lock);

  static bool first_call = true;
  if (first_call) {
    TRACE_EVENT0("sql", "EnsureSqliteInitialized");
    sqlite3_initialize();
    first_call = false;
  }

  if (create_wrapper) {
    EnsureVfsWrapper();
  }
}

}  // namespace sql
