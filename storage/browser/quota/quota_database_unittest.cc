// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"

using storage::QuotaDatabase;

namespace content {
namespace {

const char kDBFileName[] = "quota_manager.db";

}  // namespace

// Declared to shorten the line lengths.
static const blink::mojom::StorageType kTemporary =
    blink::mojom::StorageType::kTemporary;
static const blink::mojom::StorageType kPersistent =
    blink::mojom::StorageType::kPersistent;

class QuotaDatabaseTest : public testing::Test {
 protected:
  using QuotaTableEntry = QuotaDatabase::QuotaTableEntry;
  using QuotaTableCallback = QuotaDatabase::QuotaTableCallback;
  using OriginInfoTableCallback = QuotaDatabase::OriginInfoTableCallback;

  void LazyOpen(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    EXPECT_FALSE(db.LazyOpen(false));
    ASSERT_TRUE(db.LazyOpen(true));
    EXPECT_TRUE(db.db_.get());
    EXPECT_TRUE(kDbFile.empty() || base::PathExists(kDbFile));
  }

  void Reopen(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(false));
    EXPECT_TRUE(db.db_.get());
    EXPECT_TRUE(kDbFile.empty() || base::PathExists(kDbFile));
  }

  void UpgradeSchemaV2toV5(const base::FilePath& kDbFile) {
    const QuotaTableEntry entries[] = {
        QuotaTableEntry("a", kTemporary, 1),
        QuotaTableEntry("b", kTemporary, 2),
        QuotaTableEntry("c", kPersistent, 3),
    };

    CreateV2Database(kDbFile, entries, base::size(entries));

    QuotaDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    EXPECT_TRUE(db.db_.get());

    using Verifier = EntryVerifier<QuotaTableEntry>;
    Verifier verifier(entries, entries + base::size(entries));
    EXPECT_TRUE(db.DumpQuotaTable(
        base::BindRepeating(&Verifier::Run, base::Unretained(&verifier))));
    EXPECT_TRUE(verifier.table.empty());

    EXPECT_TRUE(db.db_->DoesTableExist("EvictionInfoTable"));
    EXPECT_TRUE(db.db_->DoesIndexExist("sqlite_autoindex_EvictionInfoTable_1"));
  }

  void HostQuota(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    const char* kHost = "foo.com";
    const int kQuota1 = 13579;
    const int kQuota2 = kQuota1 + 1024;

    int64_t quota = -1;
    EXPECT_FALSE(db.GetHostQuota(kHost, kTemporary, &quota));
    EXPECT_FALSE(db.GetHostQuota(kHost, kPersistent, &quota));

    // Insert quota for temporary.
    EXPECT_TRUE(db.SetHostQuota(kHost, kTemporary, kQuota1));
    EXPECT_TRUE(db.GetHostQuota(kHost, kTemporary, &quota));
    EXPECT_EQ(kQuota1, quota);

    // Update quota for temporary.
    EXPECT_TRUE(db.SetHostQuota(kHost, kTemporary, kQuota2));
    EXPECT_TRUE(db.GetHostQuota(kHost, kTemporary, &quota));
    EXPECT_EQ(kQuota2, quota);

    // Quota for persistent must not be updated.
    EXPECT_FALSE(db.GetHostQuota(kHost, kPersistent, &quota));

    // Delete temporary storage quota.
    EXPECT_TRUE(db.DeleteHostQuota(kHost, kTemporary));
    EXPECT_FALSE(db.GetHostQuota(kHost, kTemporary, &quota));

    // Delete persistent quota by setting it to zero.
    EXPECT_TRUE(db.SetHostQuota(kHost, kPersistent, 0));
    EXPECT_FALSE(db.GetHostQuota(kHost, kPersistent, &quota));
  }

  void GlobalQuota(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    const char* kTempQuotaKey = QuotaDatabase::kTemporaryQuotaOverrideKey;
    const char* kAvailSpaceKey = QuotaDatabase::kDesiredAvailableSpaceKey;

    int64_t value = 0;
    const int64_t kValue1 = 456;
    const int64_t kValue2 = 123000;
    EXPECT_FALSE(db.GetQuotaConfigValue(kTempQuotaKey, &value));
    EXPECT_FALSE(db.GetQuotaConfigValue(kAvailSpaceKey, &value));

    EXPECT_TRUE(db.SetQuotaConfigValue(kTempQuotaKey, kValue1));
    EXPECT_TRUE(db.GetQuotaConfigValue(kTempQuotaKey, &value));
    EXPECT_EQ(kValue1, value);

    EXPECT_TRUE(db.SetQuotaConfigValue(kTempQuotaKey, kValue2));
    EXPECT_TRUE(db.GetQuotaConfigValue(kTempQuotaKey, &value));
    EXPECT_EQ(kValue2, value);

    EXPECT_TRUE(db.SetQuotaConfigValue(kAvailSpaceKey, kValue1));
    EXPECT_TRUE(db.GetQuotaConfigValue(kAvailSpaceKey, &value));
    EXPECT_EQ(kValue1, value);

    EXPECT_TRUE(db.SetQuotaConfigValue(kAvailSpaceKey, kValue2));
    EXPECT_TRUE(db.GetQuotaConfigValue(kAvailSpaceKey, &value));
    EXPECT_EQ(kValue2, value);
  }

  void OriginLastAccessTimeLRU(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    std::set<url::Origin> exceptions;
    base::Optional<url::Origin> origin;
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, nullptr, &origin));
    EXPECT_FALSE(origin.has_value());

    // TODO(crbug.com/889590): Use helper for url::Origin creation from string.
    const url::Origin kOrigin1 = url::Origin::Create(GURL("http://a/"));
    const url::Origin kOrigin2 = url::Origin::Create(GURL("http://b/"));
    const url::Origin kOrigin3 = url::Origin::Create(GURL("http://c/"));
    const url::Origin kOrigin4 = url::Origin::Create(GURL("http://p/"));

    // Adding three temporary storages, and
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin1, kTemporary, QuotaDatabase::TimeFromSqlValue(10)));
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin2, kTemporary, QuotaDatabase::TimeFromSqlValue(20)));
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin3, kTemporary, QuotaDatabase::TimeFromSqlValue(30)));

    // one persistent.
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin4, kPersistent, QuotaDatabase::TimeFromSqlValue(40)));

    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, nullptr, &origin));
    EXPECT_EQ(kOrigin1, origin);

    // Test that unlimited origins are exluded from eviction, but
    // protected origins are not excluded.
    scoped_refptr<MockSpecialStoragePolicy> policy(
        new MockSpecialStoragePolicy);
    policy->AddUnlimited(kOrigin1.GetURL());
    policy->AddProtected(kOrigin2.GetURL());
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, policy.get(), &origin));
    EXPECT_EQ(kOrigin2, origin);

    // Test that durable origins are excluded from eviction.
    policy->AddDurable(kOrigin2.GetURL());
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, policy.get(), &origin));
    EXPECT_EQ(kOrigin3, origin);

    exceptions.insert(kOrigin1);
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, nullptr, &origin));
    EXPECT_EQ(kOrigin2, origin);

    exceptions.insert(kOrigin2);
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, nullptr, &origin));
    EXPECT_EQ(kOrigin3, origin);

    exceptions.insert(kOrigin3);
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, nullptr, &origin));
    EXPECT_FALSE(origin.has_value());

    EXPECT_TRUE(
        db.SetOriginLastAccessTime(kOrigin1, kTemporary, base::Time::Now()));

    // Delete origin/type last access time information.
    EXPECT_TRUE(db.DeleteOriginInfo(kOrigin3, kTemporary));

    // Querying again to see if the deletion has worked.
    exceptions.clear();
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, nullptr, &origin));
    EXPECT_EQ(kOrigin2, origin);

    exceptions.insert(kOrigin1);
    exceptions.insert(kOrigin2);
    EXPECT_TRUE(db.GetLRUOrigin(kTemporary, exceptions, nullptr, &origin));
    EXPECT_FALSE(origin.has_value());
  }

  void OriginLastModifiedSince(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    std::set<url::Origin> origins;
    EXPECT_TRUE(db.GetOriginsModifiedSince(kTemporary, &origins, base::Time()));
    EXPECT_TRUE(origins.empty());

    const url::Origin kOrigin1 = url::Origin::Create(GURL("http://a/"));
    const url::Origin kOrigin2 = url::Origin::Create(GURL("http://b/"));
    const url::Origin kOrigin3 = url::Origin::Create(GURL("http://c/"));

    // Report last mod time for the test origins.
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin1, kTemporary, QuotaDatabase::TimeFromSqlValue(0)));
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin2, kTemporary, QuotaDatabase::TimeFromSqlValue(10)));
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin3, kTemporary, QuotaDatabase::TimeFromSqlValue(20)));

    EXPECT_TRUE(db.GetOriginsModifiedSince(kTemporary, &origins, base::Time()));
    EXPECT_EQ(3U, origins.size());
    EXPECT_EQ(1U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(kTemporary, &origins,
                                           QuotaDatabase::TimeFromSqlValue(5)));
    EXPECT_EQ(2U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kTemporary, &origins, QuotaDatabase::TimeFromSqlValue(15)));
    EXPECT_EQ(1U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(0U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kTemporary, &origins, QuotaDatabase::TimeFromSqlValue(25)));
    EXPECT_TRUE(origins.empty());

    // Update origin1's mod time but for persistent storage.
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin1, kPersistent, QuotaDatabase::TimeFromSqlValue(30)));

    // Must have no effects on temporary origins info.
    EXPECT_TRUE(db.GetOriginsModifiedSince(kTemporary, &origins,
                                           QuotaDatabase::TimeFromSqlValue(5)));
    EXPECT_EQ(2U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    // One more update for persistent origin2.
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin2, kPersistent, QuotaDatabase::TimeFromSqlValue(40)));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kPersistent, &origins, QuotaDatabase::TimeFromSqlValue(25)));
    EXPECT_EQ(2U, origins.size());
    EXPECT_EQ(1U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(0U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kPersistent, &origins, QuotaDatabase::TimeFromSqlValue(35)));
    EXPECT_EQ(1U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(0U, origins.count(kOrigin3));
  }

  void OriginLastEvicted(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    const url::Origin kOrigin1 = url::Origin::Create(GURL("http://a/"));
    const url::Origin kOrigin2 = url::Origin::Create(GURL("http://b/"));
    const url::Origin kOrigin3 = url::Origin::Create(GURL("http://c/"));

    base::Time last_eviction_time;
    EXPECT_FALSE(db.GetOriginLastEvictionTime(kOrigin1, kTemporary,
                                              &last_eviction_time));
    EXPECT_EQ(base::Time(), last_eviction_time);

    // Report last eviction time for the test origins.
    EXPECT_TRUE(db.SetOriginLastEvictionTime(
        kOrigin1, kTemporary, QuotaDatabase::TimeFromSqlValue(10)));
    EXPECT_TRUE(db.SetOriginLastEvictionTime(
        kOrigin2, kTemporary, QuotaDatabase::TimeFromSqlValue(20)));
    EXPECT_TRUE(db.SetOriginLastEvictionTime(
        kOrigin3, kTemporary, QuotaDatabase::TimeFromSqlValue(30)));

    EXPECT_TRUE(db.GetOriginLastEvictionTime(kOrigin1, kTemporary,
                                             &last_eviction_time));
    EXPECT_EQ(QuotaDatabase::TimeFromSqlValue(10), last_eviction_time);
    EXPECT_TRUE(db.GetOriginLastEvictionTime(kOrigin2, kTemporary,
                                             &last_eviction_time));
    EXPECT_EQ(QuotaDatabase::TimeFromSqlValue(20), last_eviction_time);
    EXPECT_TRUE(db.GetOriginLastEvictionTime(kOrigin3, kTemporary,
                                             &last_eviction_time));
    EXPECT_EQ(QuotaDatabase::TimeFromSqlValue(30), last_eviction_time);

    // Delete last eviction times for the test origins.
    EXPECT_TRUE(db.DeleteOriginLastEvictionTime(kOrigin1, kTemporary));
    EXPECT_TRUE(db.DeleteOriginLastEvictionTime(kOrigin2, kTemporary));
    EXPECT_TRUE(db.DeleteOriginLastEvictionTime(kOrigin3, kTemporary));

    last_eviction_time = base::Time();
    EXPECT_FALSE(db.GetOriginLastEvictionTime(kOrigin1, kTemporary,
                                              &last_eviction_time));
    EXPECT_EQ(base::Time(), last_eviction_time);
    EXPECT_FALSE(db.GetOriginLastEvictionTime(kOrigin2, kTemporary,
                                              &last_eviction_time));
    EXPECT_EQ(base::Time(), last_eviction_time);
    EXPECT_FALSE(db.GetOriginLastEvictionTime(kOrigin3, kTemporary,
                                              &last_eviction_time));
    EXPECT_EQ(base::Time(), last_eviction_time);

    // Deleting an origin that is not present should not fail.
    EXPECT_TRUE(db.DeleteOriginLastEvictionTime(
        url::Origin::Create(GURL("http://notpresent.com")), kTemporary));
  }

  void RegisterInitialOriginInfo(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);

    const url::Origin kOrigins[] = {url::Origin::Create(GURL("http://a/")),
                                    url::Origin::Create(GURL("http://b/")),
                                    url::Origin::Create(GURL("http://c/"))};
    std::set<url::Origin> origins(kOrigins, kOrigins + base::size(kOrigins));

    EXPECT_TRUE(db.RegisterInitialOriginInfo(origins, kTemporary));

    QuotaDatabase::OriginInfoTableEntry info;
    info.used_count = -1;
    EXPECT_TRUE(db.GetOriginInfo(url::Origin::Create(GURL("http://a/")),
                                 kTemporary, &info));
    EXPECT_EQ(0, info.used_count);

    EXPECT_TRUE(
        db.SetOriginLastAccessTime(url::Origin::Create(GURL("http://a/")),
                                   kTemporary, base::Time::FromDoubleT(1.0)));
    info.used_count = -1;
    EXPECT_TRUE(db.GetOriginInfo(url::Origin::Create(GURL("http://a/")),
                                 kTemporary, &info));
    EXPECT_EQ(1, info.used_count);

    EXPECT_TRUE(db.RegisterInitialOriginInfo(origins, kTemporary));

    info.used_count = -1;
    EXPECT_TRUE(db.GetOriginInfo(url::Origin::Create(GURL("http://a/")),
                                 kTemporary, &info));
    EXPECT_EQ(1, info.used_count);
  }

  template <typename EntryType>
  struct EntryVerifier {
    std::set<EntryType> table;

    template <typename Iterator>
    EntryVerifier(Iterator itr, Iterator end)
        : table(itr, end) {}

    bool Run(const EntryType& entry) {
      EXPECT_EQ(1u, table.erase(entry));
      return true;
    }
  };

  void DumpQuotaTable(const base::FilePath& kDbFile) {
    QuotaTableEntry kTableEntries[] = {
        QuotaTableEntry("http://go/", kTemporary, 1),
        QuotaTableEntry("http://oo/", kTemporary, 2),
        QuotaTableEntry("http://gle/", kPersistent, 3)};
    QuotaTableEntry* begin = kTableEntries;
    QuotaTableEntry* end = kTableEntries + base::size(kTableEntries);

    QuotaDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    AssignQuotaTable(db.db_.get(), begin, end);
    db.Commit();

    using Verifier = EntryVerifier<QuotaTableEntry>;
    Verifier verifier(begin, end);
    EXPECT_TRUE(db.DumpQuotaTable(
        base::BindRepeating(&Verifier::Run, base::Unretained(&verifier))));
    EXPECT_TRUE(verifier.table.empty());
  }

  void DumpOriginInfoTable(const base::FilePath& kDbFile) {
    base::Time now(base::Time::Now());
    using Entry = QuotaDatabase::OriginInfoTableEntry;
    Entry kTableEntries[] = {
        Entry(url::Origin::Create(GURL("http://go/")), kTemporary, 2147483647,
              now, now),
        Entry(url::Origin::Create(GURL("http://oo/")), kTemporary, 0, now, now),
        Entry(url::Origin::Create(GURL("http://gle/")), kTemporary, 1, now,
              now),
    };
    Entry* begin = kTableEntries;
    Entry* end = kTableEntries + base::size(kTableEntries);

    QuotaDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    AssignOriginInfoTable(db.db_.get(), begin, end);
    db.Commit();

    using Verifier = EntryVerifier<Entry>;
    Verifier verifier(begin, end);
    EXPECT_TRUE(db.DumpOriginInfoTable(
        base::BindRepeating(&Verifier::Run, base::Unretained(&verifier))));
    EXPECT_TRUE(verifier.table.empty());
  }

  void GetOriginInfo(const base::FilePath& kDbFile) {
    const url::Origin kOrigin = url::Origin::Create(GURL("http://go/"));
    using Entry = QuotaDatabase::OriginInfoTableEntry;
    Entry kTableEntries[] = {
        Entry(kOrigin, kTemporary, 100, base::Time(), base::Time())};
    Entry* begin = kTableEntries;
    Entry* end = kTableEntries + base::size(kTableEntries);

    QuotaDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    AssignOriginInfoTable(db.db_.get(), begin, end);
    db.Commit();

    {
      Entry entry;
      EXPECT_TRUE(db.GetOriginInfo(kOrigin, kTemporary, &entry));
      EXPECT_EQ(kTableEntries[0].type, entry.type);
      EXPECT_EQ(kTableEntries[0].origin, entry.origin);
      EXPECT_EQ(kTableEntries[0].used_count, entry.used_count);
      EXPECT_EQ(kTableEntries[0].last_access_time, entry.last_access_time);
      EXPECT_EQ(kTableEntries[0].last_modified_time, entry.last_modified_time);
    }

    {
      Entry entry;
      EXPECT_FALSE(
          db.GetOriginInfo(url::Origin::Create(GURL("http://notpresent.org/")),
                           kTemporary, &entry));
    }
  }

 private:
  template <typename Iterator>
  void AssignQuotaTable(sql::Database* db, Iterator itr, Iterator end) {
    ASSERT_NE(db, (sql::Database*)nullptr);
    for (; itr != end; ++itr) {
      const char* kSql =
          "INSERT INTO HostQuotaTable"
          " (host, type, quota)"
          " VALUES (?, ?, ?)";
      sql::Statement statement;
      statement.Assign(db->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindString(0, itr->host);
      statement.BindInt(1, static_cast<int>(itr->type));
      statement.BindInt64(2, itr->quota);
      EXPECT_TRUE(statement.Run());
    }
  }

  template <typename Iterator>
  void AssignOriginInfoTable(sql::Database* db, Iterator itr, Iterator end) {
    ASSERT_NE(db, (sql::Database*)nullptr);
    for (; itr != end; ++itr) {
      const char* kSql =
          "INSERT INTO OriginInfoTable"
          " (origin, type, used_count, last_access_time, last_modified_time)"
          " VALUES (?, ?, ?, ?, ?)";
      sql::Statement statement;
      statement.Assign(db->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindString(0, itr->origin.GetURL().spec());
      statement.BindInt(1, static_cast<int>(itr->type));
      statement.BindInt(2, itr->used_count);
      statement.BindInt64(3,
                          QuotaDatabase::TimeToSqlValue(itr->last_access_time));
      statement.BindInt64(
          4, QuotaDatabase::TimeToSqlValue(itr->last_modified_time));
      EXPECT_TRUE(statement.Run());
    }
  }

  bool OpenDatabase(sql::Database* db, const base::FilePath& kDbFile) {
    if (kDbFile.empty()) {
      return db->OpenInMemory();
    }
    if (!base::CreateDirectory(kDbFile.DirName()))
      return false;
    if (!db->Open(kDbFile))
      return false;
    db->Preload();
    return true;
  }

  // Create V2 database and populate some data.
  void CreateV2Database(
      const base::FilePath& kDbFile,
      const QuotaTableEntry* entries,
      size_t entries_size) {
    std::unique_ptr<sql::Database> db(new sql::Database);
    std::unique_ptr<sql::MetaTable> meta_table(new sql::MetaTable);

    // V2 schema definitions.
    static const int kCurrentVersion = 2;
    static const int kCompatibleVersion = 2;
    static const char kHostQuotaTable[] = "HostQuotaTable";
    static const char kOriginLastAccessTable[] = "OriginLastAccessTable";
    static const QuotaDatabase::TableSchema kTables[] = {
      { kHostQuotaTable,
        "(host TEXT NOT NULL,"
        " type INTEGER NOT NULL,"
        " quota INTEGER,"
        " UNIQUE(host, type))" },
      { kOriginLastAccessTable,
        "(origin TEXT NOT NULL,"
        " type INTEGER NOT NULL,"
        " used_count INTEGER,"
        " last_access_time INTEGER,"
        " UNIQUE(origin, type))" },
    };
    static const QuotaDatabase::IndexSchema kIndexes[] = {
      { "HostIndex",
        kHostQuotaTable,
        "(host)",
        false },
      { "OriginLastAccessIndex",
        kOriginLastAccessTable,
        "(origin, last_access_time)",
        false },
    };

    ASSERT_TRUE(OpenDatabase(db.get(), kDbFile));
    EXPECT_TRUE(QuotaDatabase::CreateSchema(
        db.get(), meta_table.get(), kCurrentVersion, kCompatibleVersion,
        kTables, base::size(kTables), kIndexes, base::size(kIndexes)));

    // V2 and V3 QuotaTable are compatible, so we can simply use
    // AssignQuotaTable to poplulate v2 database here.
    db->BeginTransaction();
    AssignQuotaTable(db.get(), entries, entries + entries_size);
    db->CommitTransaction();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(QuotaDatabaseTest, LazyOpen) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  LazyOpen(kDbFile);
  LazyOpen(base::FilePath());
}

TEST_F(QuotaDatabaseTest, UpgradeSchema) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  UpgradeSchemaV2toV5(kDbFile);
}

TEST_F(QuotaDatabaseTest, HostQuota) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  HostQuota(kDbFile);
  HostQuota(base::FilePath());
}

TEST_F(QuotaDatabaseTest, GlobalQuota) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  GlobalQuota(kDbFile);
  GlobalQuota(base::FilePath());
}

TEST_F(QuotaDatabaseTest, OriginLastAccessTimeLRU) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  OriginLastAccessTimeLRU(kDbFile);
  OriginLastAccessTimeLRU(base::FilePath());
}

TEST_F(QuotaDatabaseTest, OriginLastModifiedSince) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  OriginLastModifiedSince(kDbFile);
  OriginLastModifiedSince(base::FilePath());
}

TEST_F(QuotaDatabaseTest, OriginLastEvicted) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  OriginLastEvicted(kDbFile);
  OriginLastEvicted(base::FilePath());
}

TEST_F(QuotaDatabaseTest, BootstrapFlag) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());

  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  QuotaDatabase db(kDbFile);

  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(true));
  EXPECT_TRUE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(false));
  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
}

TEST_F(QuotaDatabaseTest, RegisterInitialOriginInfo) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  RegisterInitialOriginInfo(kDbFile);
  RegisterInitialOriginInfo(base::FilePath());
}

TEST_F(QuotaDatabaseTest, DumpQuotaTable) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  DumpQuotaTable(kDbFile);
  DumpQuotaTable(base::FilePath());
}

TEST_F(QuotaDatabaseTest, DumpOriginInfoTable) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  DumpOriginInfoTable(kDbFile);
  DumpOriginInfoTable(base::FilePath());
}

TEST_F(QuotaDatabaseTest, GetOriginInfo) {
  GetOriginInfo(base::FilePath());
}

TEST_F(QuotaDatabaseTest, OpenCorruptedDatabase) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.GetPath().AppendASCII(kDBFileName);
  LazyOpen(kDbFile);
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(kDbFile));
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    Reopen(kDbFile);
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

}  // namespace content
