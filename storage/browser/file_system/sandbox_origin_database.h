// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_ORIGIN_DATABASE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_ORIGIN_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "storage/browser/file_system/sandbox_origin_database_interface.h"

namespace leveldb {
class DB;
class Env;
class Status;
}  // namespace leveldb

namespace base {
class Location;
}

namespace storage {

// All methods of this class other than the constructor may be used only from
// the browser's FILE thread.  The constructor may be used on any thread.
class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxOriginDatabase
    : public SandboxOriginDatabaseInterface {
 public:
  // Only one instance of SandboxOriginDatabase should exist for a given path
  // at a given time.
  SandboxOriginDatabase(const base::FilePath& file_system_directory,
                        leveldb::Env* env_override);
  ~SandboxOriginDatabase() override;

  // SandboxOriginDatabaseInterface overrides.
  bool HasOriginPath(const std::string& origin) override;
  bool GetPathForOrigin(const std::string& origin,
                        base::FilePath* directory) override;
  bool RemovePathForOrigin(const std::string& origin) override;
  bool ListAllOrigins(std::vector<OriginRecord>* origins) override;
  void RewriteDatabase() override;
  void DropDatabase() override;

  base::FilePath GetDatabasePath() const;
  void RemoveDatabase();

 private:
  enum RecoveryOption {
    REPAIR_ON_CORRUPTION,
    DELETE_ON_CORRUPTION,
    FAIL_ON_CORRUPTION,
  };

  enum InitOption {
    CREATE_IF_NONEXISTENT,
    FAIL_IF_NONEXISTENT,
  };

  bool Init(InitOption init_option, RecoveryOption recovery_option);
  bool RepairDatabase(const std::string& db_path);
  // Close the database. Before this, all iterators associated with the database
  // must be deleted.
  void HandleError(const base::Location& from_here,
                   const leveldb::Status& status);
  void ReportInitStatus(const leveldb::Status& status);
  bool GetLastPathNumber(int* number);

  base::FilePath file_system_directory_;
  leveldb::Env* env_override_;
  std::unique_ptr<leveldb::DB> db_;
  base::Time last_reported_time_;
  DISALLOW_COPY_AND_ASSIGN(SandboxOriginDatabase);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_ORIGIN_DATABASE_H_
