// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_INITIALIZATION_H_
#define SQL_INITIALIZATION_H_

namespace sql {

// Makes sure that sqlite3_initialize() is called.
//
// Only for use within //sql.
//
// When `create_wrapper` is true, this will create a functionality-modifying
// wrapper VFS and install it as the default. See `CreateVfsWrapper()`.
void EnsureSqliteInitialized(bool create_wrapper = true);

}  // namespace sql

#endif  // SQL_INITIALIZATION_H_
