// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_DATABASE_DATABASE_CONNECTIONS_H_
#define STORAGE_COMMON_DATABASE_DATABASE_CONNECTIONS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "storage/common/storage_common_export.h"

namespace storage {

class STORAGE_COMMON_EXPORT DatabaseConnections {
 public:
  DatabaseConnections();
  ~DatabaseConnections();

  bool IsEmpty() const;
  bool IsDatabaseOpened(const std::string& origin_identifier,
                        const base::string16& database_name) const;
  bool IsOriginUsed(const std::string& origin_identifier) const;

  // Returns true if this is the first connection.
  bool AddConnection(const std::string& origin_identifier,
                     const base::string16& database_name);

  // Returns true if the last connection was removed.
  bool RemoveConnection(const std::string& origin_identifier,
                        const base::string16& database_name);

  void RemoveAllConnections();
  std::vector<std::pair<std::string, base::string16>> RemoveConnections(
      const DatabaseConnections& connections);

  // Can be called only if IsDatabaseOpened would have returned true.
  int64_t GetOpenDatabaseSize(const std::string& origin_identifier,
                              const base::string16& database_name) const;
  void SetOpenDatabaseSize(const std::string& origin_identifier,
                           const base::string16& database_name,
                           int64_t size);

  // Returns a list of the connections, <origin_id, name>.
  std::vector<std::pair<std::string, base::string16>> ListConnections() const;

 private:
  // Maps database names to (open count, database size).
  using DBConnections = std::map<base::string16, std::pair<int, int64_t>>;
  // Maps origin identifiers to DBConnections.
  std::map<std::string, DBConnections> connections_;

  // Returns true if the last connection was removed.
  bool RemoveConnectionsHelper(const std::string& origin_identifier,
                               const base::string16& database_name,
                               int num_connections);
};

}  // namespace storage

#endif  // STORAGE_COMMON_DATABASE_DATABASE_CONNECTIONS_H_
