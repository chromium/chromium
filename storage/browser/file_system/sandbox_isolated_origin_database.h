// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "storage/browser/file_system/sandbox_origin_database_interface.h"

namespace storage {

// This origin database implementation supports only one origin
// (therefore is expected to run very fast).
class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxIsolatedOriginDatabase
    : public SandboxOriginDatabaseInterface {
 public:
  static const base::FilePath::CharType kObsoleteOriginDirectory[];

  // Initialize this database for |origin| which makes GetPathForOrigin return
  // |origin_directory| (in |file_system_directory|).
  SandboxIsolatedOriginDatabase(const std::string& origin,
                                const base::FilePath& file_system_directory,
                                const base::FilePath& origin_directory);
  ~SandboxIsolatedOriginDatabase() override;

  // SandboxOriginDatabaseInterface overrides.
  bool HasOriginPath(const std::string& origin) override;
  bool GetPathForOrigin(const std::string& origin,
                        base::FilePath* directory) override;
  bool RemovePathForOrigin(const std::string& origin) override;
  bool ListAllOrigins(std::vector<OriginRecord>* origins) override;
  void DropDatabase() override;
  void RewriteDatabase() override;

  const std::string& origin() const { return origin_; }

 private:
  const std::string origin_;
  const base::FilePath file_system_directory_;
  const base::FilePath origin_directory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxIsolatedOriginDatabase);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_
