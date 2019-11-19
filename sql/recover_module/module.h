// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_MODULE_H_
#define SQL_RECOVER_MODULE_MODULE_H_

struct sqlite3;

namespace sql {
namespace recover {

// Registers the "recover" virtual table with a SQLite connection.
//
// Returns a SQLite error code.
int RegisterRecoverExtension(sqlite3* db);

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_MODULE_H_
