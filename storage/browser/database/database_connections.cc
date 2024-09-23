// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_connections.h"

#include <ostream>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/not_fatal_until.h"

namespace storage {

DatabaseConnections::DatabaseConnections() = default;

DatabaseConnections::~DatabaseConnections() {
  DCHECK(connections_.empty());
}

bool DatabaseConnections::IsEmpty() const {
  return connections_.empty();
}

bool DatabaseConnections::IsDatabaseOpened(
    const std::string& origin_identifier,
    const std::u16string& database_name) const {
  auto origin_it = connections_.find(origin_identifier);
  if (origin_it == connections_.end())
    return false;
  const DBConnections& origin_connections = origin_it->second;
  return (origin_connections.find(database_name) != origin_connections.end());
}

bool DatabaseConnections::IsOriginUsed(
    const std::string& origin_identifier) const {
  return base::Contains(connections_, origin_identifier);
}

bool DatabaseConnections::AddConnection(const std::string& origin_identifier,
                                        const std::u16string& database_name) {
  int& count = connections_[origin_identifier][database_name].first;
  return ++count == 1;
}

bool DatabaseConnections::RemoveConnection(
    const std::string& origin_identifier,
    const std::u16string& database_name) {
  return RemoveConnectionsHelper(origin_identifier, database_name, 1);
}

void DatabaseConnections::RemoveAllConnections() {
  connections_.clear();
}

std::vector<std::pair<std::string, std::u16string>>
DatabaseConnections::RemoveConnections(const DatabaseConnections& connections) {
  std::vector<std::pair<std::string, std::u16string>> closed_dbs;
  for (const auto& origin_connections_pair : connections.connections_) {
    const DBConnections& db_connections = origin_connections_pair.second;
    for (const auto& count_size_pair : db_connections) {
      if (RemoveConnectionsHelper(origin_connections_pair.first,
                                  count_size_pair.first,
                                  count_size_pair.second.first)) {
        closed_dbs.emplace_back(origin_connections_pair.first,
                                count_size_pair.first);
      }
    }
  }
  return closed_dbs;
}

int64_t DatabaseConnections::GetOpenDatabaseSize(
    const std::string& origin_identifier,
    const std::u16string& database_name) const {
  auto origin_it = connections_.find(origin_identifier);
  CHECK(origin_it != connections_.end(), base::NotFatalUntil::M130)
      << "Database not opened";
  auto it = origin_it->second.find(database_name);
  CHECK(it != origin_it->second.end(), base::NotFatalUntil::M130)
      << "Database not opened";
  return it->second.second;
}

void DatabaseConnections::SetOpenDatabaseSize(
    const std::string& origin_identifier,
    const std::u16string& database_name,
    int64_t size) {
  DCHECK(IsDatabaseOpened(origin_identifier, database_name));
  connections_[origin_identifier][database_name].second = size;
}

std::vector<std::pair<std::string, std::u16string>>
DatabaseConnections::ListConnections() const {
  std::vector<std::pair<std::string, std::u16string>> list;
  for (const auto& origin_connections_pair : connections_) {
    const DBConnections& db_connections = origin_connections_pair.second;
    for (const auto& count_size_pair : db_connections)
      list.emplace_back(origin_connections_pair.first, count_size_pair.first);
  }
  return list;
}

bool DatabaseConnections::RemoveConnectionsHelper(
    const std::string& origin_identifier,
    const std::u16string& database_name,
    int num_connections) {
  auto origin_iterator = connections_.find(origin_identifier);
  CHECK(origin_iterator != connections_.end(), base::NotFatalUntil::M130);
  DBConnections& db_connections = origin_iterator->second;
  int& count = db_connections[database_name].first;
  DCHECK(count >= num_connections);
  count -= num_connections;
  if (count)
    return false;
  db_connections.erase(database_name);
  if (db_connections.empty())
    connections_.erase(origin_iterator);
  return true;
}

}  // namespace storage
