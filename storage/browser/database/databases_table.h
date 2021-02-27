// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASES_TABLE_H_
#define STORAGE_BROWSER_DATABASE_DATABASES_TABLE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/strings/string16.h"

namespace sql {
class Database;
}

namespace storage {

struct COMPONENT_EXPORT(STORAGE_BROWSER) DatabaseDetails {
  DatabaseDetails();
  DatabaseDetails(const DatabaseDetails& other);
  ~DatabaseDetails();

  std::string origin_identifier;
  base::string16 database_name;
  base::string16 description;
  int64_t estimated_size;
};

class COMPONENT_EXPORT(STORAGE_BROWSER) DatabasesTable {
 public:
  explicit DatabasesTable(sql::Database* db) : db_(db) {}

  bool Init();
  int64_t GetDatabaseID(const std::string& origin_identifier,
                        const base::string16& database_name);
  bool GetDatabaseDetails(const std::string& origin_identifier,
                          const base::string16& database_name,
                          DatabaseDetails* details);
  bool InsertDatabaseDetails(const DatabaseDetails& details);
  bool UpdateDatabaseDetails(const DatabaseDetails& details);
  bool DeleteDatabaseDetails(const std::string& origin_identifier,
                             const base::string16& database_name);
  bool GetAllOriginIdentifiers(std::vector<std::string>* origin_identifiers);
  bool GetAllDatabaseDetailsForOriginIdentifier(
      const std::string& origin_identifier,
      std::vector<DatabaseDetails>* details);
  bool DeleteOriginIdentifier(const std::string& origin_identifier);
 private:
  sql::Database* db_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASES_TABLE_H_
