// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_DATABASE_TEST_PEER_H_
#define SQL_TEST_DATABASE_TEST_PEER_H_

namespace base {
class FilePath;
}  // namespace base

namespace sql {

class Database;

class DatabaseTestPeer {
 public:
  static bool AttachDatabase(Database* db,
                             const base::FilePath& other_db_path,
                             const char* attachment_point);
  static bool DetachDatabase(Database* db, const char* attachment_point);

  static bool EnableRecoveryExtension(Database* db);
};

}  // namespace sql

#endif  // SQL_TEST_DATABASE_TEST_PEER_H_
