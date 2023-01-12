// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "storage/browser/database/databases_table.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace storage {

static void CheckDetailsAreEqual(const DatabaseDetails& d1,
                                 const DatabaseDetails& d2) {
  EXPECT_EQ(d1.origin_identifier, d2.origin_identifier);
  EXPECT_EQ(d1.database_name, d2.database_name);
  EXPECT_EQ(d1.description, d2.description);
}

static bool DatabasesTableIsEmpty(sql::Database* db) {
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(*) FROM Databases"));
  return (statement.is_valid() && statement.Step() && !statement.ColumnInt(0));
}

TEST(DatabasesTableTest, TestIt) {
  // Initialize the 'Databases' table.
  sql::Database db;

  sql::test::ScopedErrorExpecter expecter;
  // TODO(shess): Suppressing SQLITE_CONSTRAINT because the code
  // expects that and handles the resulting error.  Consider revising
  // the code to use INSERT OR IGNORE (which would not throw
  // SQLITE_CONSTRAINT) and then check ChangeCount() to see if any
  // changes were made.
  expecter.ExpectError(SQLITE_CONSTRAINT);

  // Initialize the temp dir and the 'Databases' table.
  EXPECT_TRUE(db.OpenInMemory());
  DatabasesTable databases_table(&db);
  EXPECT_TRUE(databases_table.Init());

  // The 'Databases' table should be empty.
  EXPECT_TRUE(DatabasesTableIsEmpty(&db));

  // Create the details for a databases.
  DatabaseDetails details_in1;
  DatabaseDetails details_out1;
  details_in1.origin_identifier = "origin1";
  details_in1.database_name = u"db1";
  details_in1.description = u"description_db1";

  // Updating details for this database should fail.
  EXPECT_FALSE(databases_table.UpdateDatabaseDetails(details_in1));
  EXPECT_FALSE(databases_table.GetDatabaseDetails(
      details_in1.origin_identifier, details_in1.database_name, &details_out1));

  // Inserting details for this database should pass.
  EXPECT_TRUE(databases_table.InsertDatabaseDetails(details_in1));
  EXPECT_TRUE(databases_table.GetDatabaseDetails(
      details_in1.origin_identifier, details_in1.database_name, &details_out1));
  EXPECT_EQ(1, databases_table.GetDatabaseID(details_in1.origin_identifier,
                                             details_in1.database_name));

  // Check that the details were correctly written to the database.
  CheckDetailsAreEqual(details_in1, details_out1);

  // Check that inserting a duplicate row fails.
  EXPECT_FALSE(databases_table.InsertDatabaseDetails(details_in1));

  // Insert details for another database with the same origin.
  DatabaseDetails details_in2;
  details_in2.origin_identifier = "origin1";
  details_in2.database_name = u"db2";
  details_in2.description = u"description_db2";
  EXPECT_TRUE(databases_table.InsertDatabaseDetails(details_in2));
  EXPECT_EQ(2, databases_table.GetDatabaseID(details_in2.origin_identifier,
                                             details_in2.database_name));

  // Insert details for a third database with a different origin.
  DatabaseDetails details_in3;
  details_in3.origin_identifier = "origin2";
  details_in3.database_name = u"db3";
  details_in3.description = u"description_db3";
  EXPECT_TRUE(databases_table.InsertDatabaseDetails(details_in3));
  EXPECT_EQ(3, databases_table.GetDatabaseID(details_in3.origin_identifier,
                                             details_in3.database_name));

  // There should be no database with origin "origin3".
  std::vector<DatabaseDetails> details_out_origin3;
  EXPECT_TRUE(databases_table.GetAllDatabaseDetailsForOriginIdentifier(
      "origin3", &details_out_origin3));
  EXPECT_TRUE(details_out_origin3.empty());

  // There should be only two databases with origin "origin1".
  std::vector<DatabaseDetails> details_out_origin1;
  EXPECT_TRUE(databases_table.GetAllDatabaseDetailsForOriginIdentifier(
      details_in1.origin_identifier, &details_out_origin1));
  EXPECT_EQ(size_t(2), details_out_origin1.size());
  CheckDetailsAreEqual(details_in1, details_out_origin1[0]);
  CheckDetailsAreEqual(details_in2, details_out_origin1[1]);

  // Get the list of all origins: should be "origin1" and "origin2".
  std::vector<std::string> origins_out;
  EXPECT_TRUE(databases_table.GetAllOriginIdentifiers(&origins_out));
  EXPECT_EQ(size_t(2), origins_out.size());
  EXPECT_EQ(details_in1.origin_identifier, origins_out[0]);
  EXPECT_EQ(details_in3.origin_identifier, origins_out[1]);

  // Delete an origin and check that it's no longer in the table.
  origins_out.clear();
  EXPECT_TRUE(
      databases_table.DeleteOriginIdentifier(details_in3.origin_identifier));
  EXPECT_TRUE(databases_table.GetAllOriginIdentifiers(&origins_out));
  EXPECT_EQ(size_t(1), origins_out.size());
  EXPECT_EQ(details_in1.origin_identifier, origins_out[0]);

  // Deleting an origin that doesn't have any record in this table should fail.
  EXPECT_FALSE(databases_table.DeleteOriginIdentifier("unknown_origin"));

  // Delete the details for 'db1' and check that they're no longer there.
  EXPECT_TRUE(databases_table.DeleteDatabaseDetails(
      details_in1.origin_identifier, details_in1.database_name));
  EXPECT_FALSE(databases_table.GetDatabaseDetails(
      details_in1.origin_identifier, details_in1.database_name, &details_out1));

  // Check that trying to delete a record that doesn't exist fails.
  EXPECT_FALSE(databases_table.DeleteDatabaseDetails("unknown_origin",
                                                     u"unknown_database"));

  ASSERT_TRUE(expecter.SawExpectedErrors());
}

}  // namespace storage
