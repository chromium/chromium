// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "storage/browser/database/database_connections.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

TEST(DatabaseConnectionsTest, DatabaseConnectionsTest) {
  const std::string kOriginId("origin_id");
  const std::u16string kName(u"database_name");
  const std::u16string kName2(u"database_name2");
  const int64_t kSize = 1000;

  DatabaseConnections connections;

  EXPECT_TRUE(connections.IsEmpty());
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_FALSE(connections.IsOriginUsed(kOriginId));

  connections.AddConnection(kOriginId, kName);
  EXPECT_FALSE(connections.IsEmpty());
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_TRUE(connections.IsOriginUsed(kOriginId));
  EXPECT_EQ(0, connections.GetOpenDatabaseSize(kOriginId, kName));
  connections.SetOpenDatabaseSize(kOriginId, kName, kSize);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));

  connections.RemoveConnection(kOriginId, kName);
  EXPECT_TRUE(connections.IsEmpty());
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_FALSE(connections.IsOriginUsed(kOriginId));

  connections.AddConnection(kOriginId, kName);
  connections.SetOpenDatabaseSize(kOriginId, kName, kSize);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  connections.AddConnection(kOriginId, kName);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  EXPECT_FALSE(connections.IsEmpty());
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_TRUE(connections.IsOriginUsed(kOriginId));
  connections.AddConnection(kOriginId, kName2);
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName2));

  DatabaseConnections another;
  another.AddConnection(kOriginId, kName);
  another.AddConnection(kOriginId, kName2);

  std::vector<std::pair<std::string, std::u16string>> closed_dbs =
      connections.RemoveConnections(another);
  EXPECT_EQ(1u, closed_dbs.size());
  EXPECT_EQ(kOriginId, closed_dbs[0].first);
  EXPECT_EQ(kName2, closed_dbs[0].second);
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName2));
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  another.RemoveAllConnections();
  connections.RemoveAllConnections();
  EXPECT_TRUE(connections.IsEmpty());

  // Ensure the return value properly indicates the initial
  // addition and final removal.
  EXPECT_TRUE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.RemoveConnection(kOriginId, kName));
  EXPECT_FALSE(connections.RemoveConnection(kOriginId, kName));
  EXPECT_TRUE(connections.RemoveConnection(kOriginId, kName));
}

}  // namespace storage
