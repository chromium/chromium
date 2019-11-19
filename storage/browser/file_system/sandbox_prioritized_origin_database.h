// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_PRIORITIZED_ORIGIN_DATABASE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_PRIORITIZED_ORIGIN_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "storage/browser/file_system/sandbox_origin_database_interface.h"

namespace leveldb {
class Env;
}

namespace storage {

class ObfuscatedFileUtil;
class SandboxIsolatedOriginDatabase;
class SandboxOriginDatabase;

class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxPrioritizedOriginDatabase
    : public SandboxOriginDatabaseInterface {
 public:
  static const base::FilePath::CharType* const kPrimaryDirectory;
  static const base::FilePath::CharType* const kPrimaryOriginFile;

  SandboxPrioritizedOriginDatabase(const base::FilePath& file_system_directory,
                                   leveldb::Env* env_override);
  ~SandboxPrioritizedOriginDatabase() override;

  // Sets |origin| as primary origin in this database (e.g. may
  // allow faster access).
  // Returns false if this database already has a primary origin
  // which is different from |origin|.
  bool InitializePrimaryOrigin(const std::string& origin);
  std::string GetPrimaryOrigin();

  // SandboxOriginDatabaseInterface overrides.
  bool HasOriginPath(const std::string& origin) override;
  bool GetPathForOrigin(const std::string& origin,
                        base::FilePath* directory) override;
  bool RemovePathForOrigin(const std::string& origin) override;
  bool ListAllOrigins(std::vector<OriginRecord>* origins) override;
  void DropDatabase() override;
  void RewriteDatabase() override;

  const base::FilePath& primary_origin_file() const {
    return primary_origin_file_;
  }

 private:
  bool MaybeLoadPrimaryOrigin();
  bool ResetPrimaryOrigin(const std::string& origin);
  void MaybeMigrateDatabase(const std::string& origin);
  void MaybeInitializeDatabases(bool create);
  void MaybeInitializeNonPrimaryDatabase(bool create);

  // For migration.
  friend class ObfuscatedFileUtil;
  SandboxOriginDatabase* GetSandboxOriginDatabase();

  const base::FilePath file_system_directory_;
  leveldb::Env* env_override_;
  const base::FilePath primary_origin_file_;
  std::unique_ptr<SandboxOriginDatabase> origin_database_;
  std::unique_ptr<SandboxIsolatedOriginDatabase> primary_origin_database_;

  DISALLOW_COPY_AND_ASSIGN(SandboxPrioritizedOriginDatabase);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_PRIORITIZED_ORIGIN_DATABASE_H_
