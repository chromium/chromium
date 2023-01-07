// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_VFS_WRAPPER_FUCHSIA_H_
#define SQL_VFS_WRAPPER_FUCHSIA_H_

#include "third_party/sqlite/sqlite3.h"

namespace sql {

// Fuchsia doesn't provide a file locking mechanism like flock(). These
// functions are used to simulate file locking. On Fuchsia profile directories
// are not expected to be shared with other processes and therefore only one
// browser process may access sqlite files. These functions are designed to
// handle the case when the same sqlite database is open more than once from the
// same browser process. In most cases databases do not need to be open more
// than once, i.e. contention is expected to be rare, so the main goal of the
// design is simplicity and not performance. The manager maintains a list of all
// currently locked files.
int Lock(sqlite3_file* sqlite_file, int file_lock);
int Unlock(sqlite3_file* sqlite_file, int file_lock);
int CheckReservedLock(sqlite3_file* sqlite_file, int* result);

}  // namespace sql

#endif  // SQL_VFS_WRAPPER_FUCHSIA_H_
