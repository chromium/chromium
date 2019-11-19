// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_DATABASE_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "content/browser/dom_storage/dom_storage_types.h"
#include "content/common/content_export.h"
#include "sql/database.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace content {

// Represents a SQLite based backing for DOM storage data. This
// class is designed to be used on a single thread.
class CONTENT_EXPORT DOMStorageDatabase {
 public:
  explicit DOMStorageDatabase(const base::FilePath& file_path);
  virtual ~DOMStorageDatabase();  // virtual for unit testing

  // Reads all the key, value pairs stored in the database and returns
  // them. |result| is assumed to be empty and any duplicate keys will
  // be overwritten. If the database exists on disk then it will be
  // opened. If it does not exist then it will not be created and
  // |result| will be unmodified.
  void ReadAllValues(DOMStorageValuesMap* result);

  // Updates the backing database. Will remove all keys before updating
  // the database if |clear_all_first| is set. Then all entries in
  // |changes| will be examined - keys mapped to a null NullableString16
  // will be removed and all others will be inserted/updated as appropriate.
  bool CommitChanges(bool clear_all_first, const DOMStorageValuesMap& changes);

  // Adds memory statistics of the database to |pmd| for tracing.
  void ReportMemoryUsage(base::trace_event::ProcessMemoryDump* pmd,
                         const std::string& name);

  // Simple getter for the path we were constructed with.
  const base::FilePath& file_path() const { return file_path_; }

 protected:
  // Constructor that uses an in-memory sqlite database, for testing.
  DOMStorageDatabase();

 private:
  friend class LocalStorageDatabaseAdapter;
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest, SimpleOpenAndClose);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest, TestLazyOpenIsLazy);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest, TestDetectSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest,
                           TestLazyOpenUpgradesDatabase);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest, SimpleWriteAndReadBack);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest, WriteWithClear);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest,
                           UpgradeFromV1ToV2WithData);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest, TestSimpleRemoveOneValue);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest,
                           TestCanOpenAndReadWebCoreDatabase);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageDatabaseTest,
                           TestCanOpenFileThatIsNotADatabase);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, BackingDatabaseOpened);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaParamTest, ShallowCopyWithBacking);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, EnableDisableCachingWithBacking);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, CommitTasks);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, PurgeMemory);

  enum SchemaVersion {
    INVALID,

    // V1 is deprecated.

    // 2011-07-15 - https://bugs.webkit.org/show_bug.cgi?id=58762
    V2,
  };

  // Open the database at file_path_ if it exists already and creates it if
  // |create_if_needed| is true.
  // Ensures we are at the correct database version and creates or updates
  // tables as necessary. Returns false on failure.
  bool LazyOpen(bool create_if_needed);

  // Analyses the database to verify that the connection that is open is indeed
  // a valid database and works out the schema version.
  SchemaVersion DetectSchemaVersion();

  // Creates the database table at V2. Returns true if the table was created
  // successfully, false otherwise. Will return false if the table already
  // exists.
  bool CreateTableV2();

  // If we have issues while trying to open the file (corrupted databse,
  // failing to upgrade, that sort of thing) this function will remove
  // the file from disk and attempt to create a new database from
  // scratch.
  bool DeleteFileAndRecreate();

  void Close();
  bool IsOpen() const { return db_.get() ? db_->is_open() : false; }

  // Initialization code shared between the two constructors of this class.
  void Init();

  // Path to the database on disk.
  const base::FilePath file_path_;
  std::unique_ptr<sql::Database> db_;
  bool failed_to_open_;
  bool tried_to_recreate_;
  bool known_to_be_empty_;

  DISALLOW_COPY_AND_ASSIGN(DOMStorageDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
