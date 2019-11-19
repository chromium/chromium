// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/database_test_peer.h"

#include "base/files/file_path.h"
#include "sql/database.h"
#include "sql/internal_api_token.h"
#include "sql/recovery.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// static
bool DatabaseTestPeer::AttachDatabase(Database* db,
                                      const base::FilePath& other_db_path,
                                      const char* attachment_point) {
  return db->AttachDatabase(other_db_path, attachment_point,
                            InternalApiToken());
}

// static
bool DatabaseTestPeer::DetachDatabase(Database* db,
                                      const char* attachment_point) {
  return db->DetachDatabase(attachment_point, InternalApiToken());
}

// static
bool DatabaseTestPeer::EnableRecoveryExtension(Database* db) {
  return Recovery::EnableRecoveryExtension(db, InternalApiToken()) == SQLITE_OK;
}

}  // namespace sql
