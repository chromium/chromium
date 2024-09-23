// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/sqlite/sqlite3.h"

namespace storage {

const char kOrigin1Url[] = "http://origin1";
const char kOrigin2Url[] = "http://origin2";

class TestObserver : public DatabaseTracker::Observer {
 public:
  TestObserver()
      : new_notification_received_(false),
        observe_size_changes_(true),
        observe_scheduled_deletions_(true) {}
  TestObserver(bool observe_size_changes, bool observe_scheduled_deletions)
      : new_notification_received_(false),
        observe_size_changes_(observe_size_changes),
        observe_scheduled_deletions_(observe_scheduled_deletions) {}

  ~TestObserver() override = default;
  void OnDatabaseSizeChanged(const std::string& origin_identifier,
                             const std::u16string& database_name,
                             int64_t database_size) override {
    if (!observe_size_changes_)
      return;
    new_notification_received_ = true;
    origin_identifier_ = origin_identifier;
    database_name_ = database_name;
    database_size_ = database_size;
  }
  void OnDatabaseScheduledForDeletion(
      const std::string& origin_identifier,
      const std::u16string& database_name) override {
    if (!observe_scheduled_deletions_)
      return;
    new_notification_received_ = true;
    origin_identifier_ = origin_identifier;
    database_name_ = database_name;
  }
  bool DidReceiveNewNotification() {
    bool temp_new_notification_received = new_notification_received_;
    new_notification_received_ = false;
    return temp_new_notification_received;
  }
  std::string GetNotificationOriginIdentifier() { return origin_identifier_; }
  std::u16string GetNotificationDatabaseName() { return database_name_; }
  int64_t GetNotificationDatabaseSize() { return database_size_; }

 private:
  bool new_notification_received_;
  bool observe_size_changes_;
  bool observe_scheduled_deletions_;
  std::string origin_identifier_;
  std::u16string database_name_;
  int64_t database_size_;
};

void CheckNotificationReceived(TestObserver* observer,
                               const std::string& expected_origin_identifier,
                               const std::u16string& expected_database_name,
                               int64_t expected_database_size) {
  EXPECT_TRUE(observer->DidReceiveNewNotification());
  EXPECT_EQ(expected_origin_identifier,
            observer->GetNotificationOriginIdentifier());
  EXPECT_EQ(expected_database_name, observer->GetNotificationDatabaseName());
  EXPECT_EQ(expected_database_size, observer->GetNotificationDatabaseSize());
}

// Must be destroyed on the sequence that called RegisterClient() most recently.
class TestQuotaManagerProxy : public QuotaManagerProxy {
 public:
  TestQuotaManagerProxy()
      : QuotaManagerProxy(
            /*quota_manager_impl=*/nullptr,
            base::SequencedTaskRunner::GetCurrentDefault(),
            /*profile_path=*/base::FilePath()) {}

  void RegisterClient(
      mojo::PendingRemote<mojom::QuotaClient> client,
      QuotaClientType client_type,
      const base::flat_set<blink::mojom::StorageType>& storage_types) override {
    EXPECT_FALSE(registered_client_);
    registered_client_.Bind(std::move(client));
  }

  void NotifyBucketAccessed(const BucketLocator& bucket,
                            base::Time access_time) override {
    EXPECT_EQ(blink::mojom::StorageType::kTemporary, bucket.type);
    accesses_[bucket.storage_key] += 1;
  }

  void NotifyBucketModified(
      QuotaClientType client_id,
      const BucketLocator& bucket,
      std::optional<int64_t> delta,
      base::Time modification_time,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceClosure callback) override {
    EXPECT_EQ(QuotaClientType::kDatabase, client_id);
    EXPECT_EQ(blink::mojom::StorageType::kTemporary, bucket.type);
    modifications_[bucket.storage_key].first += 1;
    modifications_[bucket.storage_key].second += delta.value_or(0);
    if (callback)
      callback_task_runner->PostTask(FROM_HERE, std::move(callback));
  }

  // Not needed for our tests.
  void SetUsageCacheEnabled(QuotaClientType client_id,
                            const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            bool enabled) override {}
  void GetUsageAndQuota(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      UsageAndQuotaCallback callback) override {}

  bool WasAccessNotified(const blink::StorageKey& storage_key) {
    return accesses_[storage_key] != 0;
  }

  bool WasModificationNotified(const blink::StorageKey& storage_key,
                               int64_t amount) {
    return modifications_[storage_key].first != 0 &&
           modifications_[storage_key].second == amount;
  }

  void ResetRecordedTestState() {
    accesses_.clear();
    modifications_.clear();
  }

  mojo::Remote<mojom::QuotaClient> registered_client_;

  // Map from storage key to count of access notifications.
  std::map<blink::StorageKey, int> accesses_;

  // Map from storage key to <count, sum of deltas>
  std::map<blink::StorageKey, std::pair<int, int64_t>> modifications_;

 protected:
  ~TestQuotaManagerProxy() override = default;
};

bool EnsureFileOfSize(const base::FilePath& file_path, int64_t length) {
  base::File file(file_path,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;
  return file.SetLength(length);
}

// We declare a helper class, and make it a friend of DatabaseTracker using
// the FORWARD_DECLARE_TEST macro, and we implement all tests we want to run as
// static methods of this class. Then we make our TEST() targets call these
// static functions. This allows us to run each test in normal mode and
// incognito mode without writing the same code twice.
class DatabaseTracker_TestHelper_Test {
 public:
  static void TestDeleteOpenDatabase(bool incognito_mode) {
    // Initialize the tracker database.
    base::test::TaskEnvironment task_environment;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<DatabaseTracker> tracker = DatabaseTracker::Create(
        temp_dir.GetPath(), incognito_mode, /*quota_manager_proxy=*/nullptr);

    base::RunLoop run_loop;
    tracker->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          absl::Cleanup quit_runner = [&] { run_loop.Quit(); };

          // Create and open three databases.
          int64_t database_size = 0;
          const std::string kOrigin1 =
              GetIdentifierFromOrigin(GURL(kOrigin1Url));
          const std::string kOrigin2 =
              GetIdentifierFromOrigin(GURL(kOrigin2Url));
          const std::u16string kDB1 = u"db1";
          const std::u16string kDB2 = u"db2";
          const std::u16string kDB3 = u"db3";
          const std::u16string kDescription = u"database_description";

          tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, &database_size);
          tracker->DatabaseOpened(kOrigin2, kDB2, kDescription, &database_size);
          tracker->DatabaseOpened(kOrigin2, kDB3, kDescription, &database_size);

          EXPECT_TRUE(
              base::CreateDirectory(tracker->GetOriginDirectory(kOrigin1)));
          EXPECT_TRUE(
              base::CreateDirectory(tracker->GetOriginDirectory(kOrigin2)));
          EXPECT_TRUE(
              base::WriteFile(tracker->GetFullDBFilePath(kOrigin1, kDB1), "a"));
          EXPECT_TRUE(base::WriteFile(
              tracker->GetFullDBFilePath(kOrigin2, kDB2), "aa"));
          EXPECT_TRUE(base::WriteFile(
              tracker->GetFullDBFilePath(kOrigin2, kDB3), "aaa"));
          tracker->DatabaseModified(kOrigin1, kDB1);
          tracker->DatabaseModified(kOrigin2, kDB2);
          tracker->DatabaseModified(kOrigin2, kDB3);

          // Delete db1. Should also delete origin1.
          TestObserver observer;
          tracker->AddObserver(&observer);
          net::TestCompletionCallback delete_database_callback;
          tracker->DeleteDatabase(kOrigin1, kDB1,
                                  delete_database_callback.callback());
          EXPECT_FALSE(delete_database_callback.have_result());
          EXPECT_TRUE(observer.DidReceiveNewNotification());
          EXPECT_EQ(kOrigin1, observer.GetNotificationOriginIdentifier());
          EXPECT_EQ(kDB1, observer.GetNotificationDatabaseName());
          tracker->DatabaseClosed(kOrigin1, kDB1);
          EXPECT_EQ(net::OK, delete_database_callback.WaitForResult());
          EXPECT_FALSE(base::PathExists(tracker->GetOriginDirectory(kOrigin1)));

          // Recreate db1.
          tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, &database_size);
          EXPECT_TRUE(
              base::CreateDirectory(tracker->GetOriginDirectory(kOrigin1)));
          EXPECT_TRUE(
              base::WriteFile(tracker->GetFullDBFilePath(kOrigin1, kDB1), "a"));
          tracker->DatabaseModified(kOrigin1, kDB1);

          // Setup file modification times.  db1 and db2 are modified now, db3
          // three days ago.
          base::Time now = base::Time::Now();
          EXPECT_TRUE(base::TouchFile(
              tracker->GetFullDBFilePath(kOrigin1, kDB1), now, now));
          EXPECT_TRUE(base::TouchFile(
              tracker->GetFullDBFilePath(kOrigin2, kDB2), now, now));
          base::Time three_days_ago = now - base::Days(3);
          EXPECT_TRUE(
              base::TouchFile(tracker->GetFullDBFilePath(kOrigin2, kDB3),
                              three_days_ago, three_days_ago));

          // Delete databases modified since yesterday. db2 is in the allowlist.
          base::Time yesterday = base::Time::Now();
          yesterday -= base::Days(1);

          net::TestCompletionCallback delete_data_modified_since_callback;
          tracker->DeleteDataModifiedSince(
              yesterday, delete_data_modified_since_callback.callback());
          EXPECT_FALSE(delete_data_modified_since_callback.have_result());
          EXPECT_TRUE(observer.DidReceiveNewNotification());
          tracker->DatabaseClosed(kOrigin1, kDB1);
          tracker->DatabaseClosed(kOrigin2, kDB2);
          EXPECT_EQ(net::OK,
                    delete_data_modified_since_callback.WaitForResult());
          EXPECT_FALSE(base::PathExists(tracker->GetOriginDirectory(kOrigin1)));
          EXPECT_FALSE(
              base::PathExists(tracker->GetFullDBFilePath(kOrigin2, kDB2)));
          EXPECT_TRUE(
              base::PathExists(tracker->GetFullDBFilePath(kOrigin2, kDB3)));

          tracker->DatabaseClosed(kOrigin2, kDB3);
          tracker->RemoveObserver(&observer);

          tracker->Shutdown();
        }));
    run_loop.Run();
  }

  static void TestDatabaseTracker(bool incognito_mode) {
    // Initialize the tracker database.
    base::test::TaskEnvironment task_environment;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<DatabaseTracker> tracker = DatabaseTracker::Create(
        temp_dir.GetPath(), incognito_mode, /*quota_manager_proxy=*/nullptr);

    base::RunLoop run_loop;
    tracker->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          absl::Cleanup quit_runner = [&] { run_loop.Quit(); };

          // Add two observers.
          TestObserver observer1;
          TestObserver observer2;
          tracker->AddObserver(&observer1);
          tracker->AddObserver(&observer2);

          // Open three new databases.
          int64_t database_size = 0;
          const std::string kOrigin1 =
              GetIdentifierFromOrigin(GURL(kOrigin1Url));
          const std::string kOrigin2 =
              GetIdentifierFromOrigin(GURL(kOrigin2Url));
          const std::u16string kDB1 = u"db1";
          const std::u16string kDB2 = u"db2";
          const std::u16string kDB3 = u"db3";
          const std::u16string kDescription = u"database_description";

          // Get the info for kOrigin1 and kOrigin2
          DatabaseTracker::CachedOriginInfo* origin1_info =
              tracker->GetCachedOriginInfo(kOrigin1);
          DatabaseTracker::CachedOriginInfo* origin2_info =
              tracker->GetCachedOriginInfo(kOrigin1);
          EXPECT_TRUE(origin1_info);
          EXPECT_TRUE(origin2_info);

          tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, &database_size);
          EXPECT_EQ(0, database_size);
          tracker->DatabaseOpened(kOrigin2, kDB2, kDescription, &database_size);
          EXPECT_EQ(0, database_size);
          tracker->DatabaseOpened(kOrigin1, kDB3, kDescription, &database_size);
          EXPECT_EQ(0, database_size);

          // Write some data to each file and check that the listeners are
          // called with the appropriate values.
          EXPECT_TRUE(
              base::CreateDirectory(tracker->GetOriginDirectory(kOrigin1)));
          EXPECT_TRUE(
              base::CreateDirectory(tracker->GetOriginDirectory(kOrigin2)));
          EXPECT_TRUE(
              base::WriteFile(tracker->GetFullDBFilePath(kOrigin1, kDB1), "a"));
          EXPECT_TRUE(base::WriteFile(
              tracker->GetFullDBFilePath(kOrigin2, kDB2), "aa"));
          EXPECT_TRUE(base::WriteFile(
              tracker->GetFullDBFilePath(kOrigin1, kDB3), "aaaa"));
          tracker->DatabaseModified(kOrigin1, kDB1);
          CheckNotificationReceived(&observer1, kOrigin1, kDB1, 1);
          CheckNotificationReceived(&observer2, kOrigin1, kDB1, 1);
          tracker->DatabaseModified(kOrigin2, kDB2);
          CheckNotificationReceived(&observer1, kOrigin2, kDB2, 2);
          CheckNotificationReceived(&observer2, kOrigin2, kDB2, 2);
          tracker->DatabaseModified(kOrigin1, kDB3);
          CheckNotificationReceived(&observer1, kOrigin1, kDB3, 4);
          CheckNotificationReceived(&observer2, kOrigin1, kDB3, 4);

          // Close all databases
          tracker->DatabaseClosed(kOrigin1, kDB1);
          tracker->DatabaseClosed(kOrigin2, kDB2);
          tracker->DatabaseClosed(kOrigin1, kDB3);

          // Open an existing database and check the reported size
          tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, &database_size);
          EXPECT_EQ(1, database_size);
          tracker->DatabaseClosed(kOrigin1, kDB1);

          // Remove an observer; this should clear all caches.
          tracker->RemoveObserver(&observer2);

          // Close the tracker database and clear all caches.
          // Then make sure that DatabaseOpened() still returns the correct
          // result.
          tracker->CloseTrackerDatabaseAndClearCaches();
          tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, &database_size);
          EXPECT_EQ(1, database_size);
          tracker->DatabaseClosed(kOrigin1, kDB1);

          // Remove all observers.
          tracker->RemoveObserver(&observer1);

          // Trying to delete a database in use should fail
          tracker->DatabaseOpened(kOrigin1, kDB3, kDescription, &database_size);
          EXPECT_FALSE(tracker->DeleteClosedDatabase(kOrigin1, kDB3));
          origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
          EXPECT_TRUE(origin1_info);
          EXPECT_EQ(4, origin1_info->GetDatabaseSize(kDB3));
          tracker->DatabaseClosed(kOrigin1, kDB3);

          // Delete a database and make sure the space used by that origin is
          // updated.
          EXPECT_TRUE(tracker->DeleteClosedDatabase(kOrigin1, kDB3));
          origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
          EXPECT_TRUE(origin1_info);
          EXPECT_EQ(1, origin1_info->GetDatabaseSize(kDB1));
          EXPECT_EQ(0, origin1_info->GetDatabaseSize(kDB3));

          // Get all data for all origins.
          std::vector<OriginInfo> origins_info;
          EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
          EXPECT_EQ(size_t(2), origins_info.size());
          EXPECT_EQ(kOrigin1, origins_info[0].GetOriginIdentifier());
          EXPECT_EQ(1, origins_info[0].TotalSize());
          EXPECT_EQ(1, origins_info[0].GetDatabaseSize(kDB1));
          EXPECT_EQ(0, origins_info[0].GetDatabaseSize(kDB3));

          EXPECT_EQ(kOrigin2, origins_info[1].GetOriginIdentifier());
          EXPECT_EQ(2, origins_info[1].TotalSize());

          // Trying to delete an origin with databases in use should fail.
          tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, &database_size);
          EXPECT_FALSE(tracker->DeleteOrigin(kOrigin1, false));
          origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
          EXPECT_TRUE(origin1_info);
          EXPECT_EQ(1, origin1_info->GetDatabaseSize(kDB1));
          tracker->DatabaseClosed(kOrigin1, kDB1);

          // Delete an origin that doesn't have any database in use.
          EXPECT_TRUE(tracker->DeleteOrigin(kOrigin1, false));
          origins_info.clear();
          EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
          EXPECT_EQ(size_t(1), origins_info.size());
          EXPECT_EQ(kOrigin2, origins_info[0].GetOriginIdentifier());

          origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
          EXPECT_TRUE(origin1_info);
          EXPECT_EQ(0, origin1_info->TotalSize());

          tracker->Shutdown();
        }));
    run_loop.Run();
  }

  static void DatabaseTrackerQuotaIntegration(bool incognito_mode) {
    const blink::StorageKey kStorageKey =
        blink::StorageKey::CreateFromStringForTesting(kOrigin1Url);
    const std::string kOriginId = GetIdentifierFromOrigin(kStorageKey.origin());
    const std::u16string kName = u"name";
    const std::u16string kDescription = u"description";

    base::test::TaskEnvironment task_environment;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    // Initialize the tracker with a QuotaManagerProxy
    auto test_quota_proxy = base::MakeRefCounted<TestQuotaManagerProxy>();
    scoped_refptr<DatabaseTracker> tracker = DatabaseTracker::Create(
        temp_dir.GetPath(), incognito_mode, test_quota_proxy);
    base::RunLoop run_loop;
    tracker->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          absl::Cleanup quit_runner = [&] { run_loop.Quit(); };

          EXPECT_TRUE(test_quota_proxy->registered_client_);

          // Create a database and modify it a couple of times, close it,
          // then delete it. Observe the tracker notifies accordingly.

          int64_t database_size = 0;
          tracker->DatabaseOpened(kOriginId, kName, kDescription,
                                  &database_size);
          EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kStorageKey));
          test_quota_proxy->ResetRecordedTestState();

          base::FilePath db_file(tracker->GetFullDBFilePath(kOriginId, kName));
          EXPECT_FALSE(
              base::PathExists(tracker->GetOriginDirectory(kOriginId)));
          EXPECT_TRUE(base::CreateDirectory(db_file.DirName()));
          EXPECT_TRUE(EnsureFileOfSize(db_file, 10));
          EXPECT_TRUE(base::PathExists(tracker->GetOriginDirectory(kOriginId)));
          tracker->DatabaseModified(kOriginId, kName);
          EXPECT_TRUE(
              test_quota_proxy->WasModificationNotified(kStorageKey, 10));
          test_quota_proxy->ResetRecordedTestState();

          EXPECT_TRUE(EnsureFileOfSize(db_file, 100));
          tracker->DatabaseModified(kOriginId, kName);
          EXPECT_TRUE(
              test_quota_proxy->WasModificationNotified(kStorageKey, 90));
          test_quota_proxy->ResetRecordedTestState();

          tracker->DatabaseClosed(kOriginId, kName);
          EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kStorageKey));
          net::TestCompletionCallback delete_database_callback;
          tracker->DeleteDatabase(kOriginId, kName,
                                  delete_database_callback.callback());
          EXPECT_TRUE(delete_database_callback.have_result());
          EXPECT_EQ(net::OK, delete_database_callback.WaitForResult());
          EXPECT_TRUE(
              test_quota_proxy->WasModificationNotified(kStorageKey, -100));
          test_quota_proxy->ResetRecordedTestState();

          EXPECT_FALSE(
              base::PathExists(tracker->GetOriginDirectory(kOriginId)));

          // Create a database and modify it, try to delete it while open,
          // then close it (at which time deletion will actually occur).
          // Observe the tracker notifies accordingly.

          tracker->DatabaseOpened(kOriginId, kName, kDescription,
                                  &database_size);
          EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kStorageKey));
          test_quota_proxy->ResetRecordedTestState();

          db_file = tracker->GetFullDBFilePath(kOriginId, kName);
          EXPECT_FALSE(
              base::PathExists(tracker->GetOriginDirectory(kOriginId)));
          EXPECT_TRUE(base::CreateDirectory(db_file.DirName()));
          EXPECT_TRUE(EnsureFileOfSize(db_file, 100));
          EXPECT_TRUE(base::PathExists(tracker->GetOriginDirectory(kOriginId)));
          tracker->DatabaseModified(kOriginId, kName);
          EXPECT_TRUE(
              test_quota_proxy->WasModificationNotified(kStorageKey, 100));
          test_quota_proxy->ResetRecordedTestState();

          net::TestCompletionCallback delete_database_callback2;
          tracker->DeleteDatabase(kOriginId, kName,
                                  delete_database_callback2.callback());
          EXPECT_FALSE(delete_database_callback2.have_result());
          EXPECT_FALSE(
              test_quota_proxy->WasModificationNotified(kStorageKey, -100));
          EXPECT_TRUE(base::PathExists(tracker->GetOriginDirectory(kOriginId)));

          tracker->DatabaseClosed(kOriginId, kName);
          EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kStorageKey));
          EXPECT_TRUE(
              test_quota_proxy->WasModificationNotified(kStorageKey, -100));
          EXPECT_FALSE(
              base::PathExists(tracker->GetOriginDirectory(kOriginId)));
          EXPECT_TRUE(delete_database_callback2.have_result());
          EXPECT_EQ(net::OK, delete_database_callback2.WaitForResult());
          test_quota_proxy->ResetRecordedTestState();

          // Create a database and up the file size without telling
          // the tracker about the modification, than simulate a
          // a renderer crash.
          // Observe the tracker notifies accordingly.

          tracker->DatabaseOpened(kOriginId, kName, kDescription,
                                  &database_size);
          EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kStorageKey));
          test_quota_proxy->ResetRecordedTestState();
          db_file = tracker->GetFullDBFilePath(kOriginId, kName);
          EXPECT_FALSE(
              base::PathExists(tracker->GetOriginDirectory(kOriginId)));
          EXPECT_TRUE(base::CreateDirectory(db_file.DirName()));
          EXPECT_TRUE(base::PathExists(tracker->GetOriginDirectory(kOriginId)));
          EXPECT_TRUE(EnsureFileOfSize(db_file, 100));
          DatabaseConnections crashed_renderer_connections;
          crashed_renderer_connections.AddConnection(kOriginId, kName);
          EXPECT_FALSE(
              test_quota_proxy->WasModificationNotified(kStorageKey, 100));
          tracker->CloseDatabases(crashed_renderer_connections);
          EXPECT_TRUE(
              test_quota_proxy->WasModificationNotified(kStorageKey, 100));

          // Cleanup.
          crashed_renderer_connections.RemoveAllConnections();
          tracker->Shutdown();
        }));
    run_loop.Run();
  }

  static void EmptyDatabaseNameIsValid() {
    const GURL kOrigin(kOrigin1Url);
    const std::string kOriginId = GetIdentifierFromOrigin(kOrigin);
    const std::u16string kEmptyName;
    const std::u16string kDescription(u"description");
    const std::u16string kChangedDescription(u"changed_description");

    // Initialize a tracker database, no need to put it on disk.
    const bool kUseInMemoryTrackerDatabase = true;
    base::test::TaskEnvironment task_environment;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<DatabaseTracker> tracker =
        DatabaseTracker::Create(temp_dir.GetPath(), kUseInMemoryTrackerDatabase,
                                /*quota_manager_proxy=*/nullptr);
    base::RunLoop run_loop;
    tracker->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          absl::Cleanup quit_runner = [&] { run_loop.Quit(); };

          // Starts off with no databases.
          std::vector<OriginInfo> infos;
          EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
          EXPECT_TRUE(infos.empty());

          // Create a db with an empty name.
          int64_t database_size = -1;
          tracker->DatabaseOpened(kOriginId, kEmptyName, kDescription,
                                  &database_size);
          EXPECT_EQ(0, database_size);
          tracker->DatabaseModified(kOriginId, kEmptyName);
          EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
          EXPECT_EQ(1u, infos.size());
          EXPECT_FALSE(
              tracker->GetFullDBFilePath(kOriginId, kEmptyName).empty());
          tracker->DatabaseOpened(kOriginId, kEmptyName, kChangedDescription,
                                  &database_size);
          infos.clear();
          EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
          EXPECT_EQ(1u, infos.size());
          tracker->DatabaseClosed(kOriginId, kEmptyName);
          tracker->DatabaseClosed(kOriginId, kEmptyName);

          // Deleting it should return to the initial state.
          net::TestCompletionCallback delete_database_callback;
          tracker->DeleteDatabase(kOriginId, kEmptyName,
                                  delete_database_callback.callback());
          EXPECT_TRUE(delete_database_callback.have_result());
          EXPECT_EQ(net::OK, delete_database_callback.WaitForResult());
          infos.clear();
          EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
          EXPECT_TRUE(infos.empty());

          tracker->Shutdown();
        }));
    run_loop.Run();
  }

  static void HandleSqliteError() {
    const GURL kOrigin(kOrigin1Url);
    const std::string kOriginId = GetIdentifierFromOrigin(kOrigin);
    const std::u16string kName(u"name");
    const std::u16string kDescription(u"description");

    // Initialize a tracker database, no need to put it on disk.
    const bool kUseInMemoryTrackerDatabase = true;
    base::test::TaskEnvironment task_environment;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<DatabaseTracker> tracker =
        DatabaseTracker::Create(temp_dir.GetPath(), kUseInMemoryTrackerDatabase,
                                /*quota_manager_proxy=*/nullptr);
    base::RunLoop run_loop;
    tracker->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          absl::Cleanup quit_runner = [&] { run_loop.Quit(); };

          // Setup to observe OnScheduledForDelete notifications.
          TestObserver observer(false, true);
          tracker->AddObserver(&observer);

          // Verify does no harm when there is no such database.
          tracker->HandleSqliteError(kOriginId, kName, SQLITE_CORRUPT);
          EXPECT_FALSE(
              tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
          EXPECT_FALSE(observer.DidReceiveNewNotification());

          // --------------------------------------------------------
          // Create a record of a database in the tracker db and create
          // a spoof_db_file on disk in the expected location.
          int64_t database_size = 0;
          tracker->DatabaseOpened(kOriginId, kName, kDescription,
                                  &database_size);
          base::FilePath spoof_db_file =
              tracker->GetFullDBFilePath(kOriginId, kName);
          EXPECT_FALSE(tracker->GetFullDBFilePath(kOriginId, kName).empty());
          EXPECT_TRUE(base::CreateDirectory(spoof_db_file.DirName()));
          EXPECT_TRUE(EnsureFileOfSize(spoof_db_file, 1));

          // Verify does no harm with a non-error is reported.
          tracker->HandleSqliteError(kOriginId, kName, SQLITE_OK);
          EXPECT_FALSE(
              tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
          EXPECT_FALSE(observer.DidReceiveNewNotification());

          // Verify that with a connection open, the db is scheduled for
          // deletion, but that the file still exists.
          tracker->HandleSqliteError(kOriginId, kName, SQLITE_CORRUPT);
          EXPECT_TRUE(
              tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
          EXPECT_TRUE(observer.DidReceiveNewNotification());
          EXPECT_TRUE(base::PathExists(spoof_db_file));

          // Verify that once closed, the file is deleted and the record in the
          // tracker db is removed.
          tracker->DatabaseClosed(kOriginId, kName);
          EXPECT_FALSE(base::PathExists(spoof_db_file));
          EXPECT_TRUE(tracker->GetFullDBFilePath(kOriginId, kName).empty());

          // --------------------------------------------------------
          // Create another record of a database in the tracker db and create
          // a spoof_db_file on disk in the expected location.
          tracker->DatabaseOpened(kOriginId, kName, kDescription,
                                  &database_size);
          base::FilePath spoof_db_file2 =
              tracker->GetFullDBFilePath(kOriginId, kName);
          EXPECT_FALSE(tracker->GetFullDBFilePath(kOriginId, kName).empty());
          EXPECT_NE(spoof_db_file, spoof_db_file2);
          EXPECT_TRUE(base::CreateDirectory(spoof_db_file2.DirName()));
          EXPECT_TRUE(EnsureFileOfSize(spoof_db_file2, 1));

          // Verify that with no connection open, the db is deleted immediately.
          tracker->DatabaseClosed(kOriginId, kName);
          tracker->HandleSqliteError(kOriginId, kName, SQLITE_CORRUPT);
          EXPECT_FALSE(
              tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
          EXPECT_FALSE(observer.DidReceiveNewNotification());
          EXPECT_TRUE(tracker->GetFullDBFilePath(kOriginId, kName).empty());
          EXPECT_FALSE(base::PathExists(spoof_db_file2));

          tracker->RemoveObserver(&observer);

          tracker->Shutdown();
        }));
    run_loop.Run();
  }
};

TEST(DatabaseTrackerTest, DeleteOpenDatabase) {
  DatabaseTracker_TestHelper_Test::TestDeleteOpenDatabase(false);
}

TEST(DatabaseTrackerTest, DeleteOpenDatabaseIncognitoMode) {
  DatabaseTracker_TestHelper_Test::TestDeleteOpenDatabase(true);
}

TEST(DatabaseTrackerTest, DatabaseTracker) {
  DatabaseTracker_TestHelper_Test::TestDatabaseTracker(false);
}

TEST(DatabaseTrackerTest, DatabaseTrackerIncognitoMode) {
  DatabaseTracker_TestHelper_Test::TestDatabaseTracker(true);
}

TEST(DatabaseTrackerTest, DatabaseTrackerQuotaIntegration) {
  DatabaseTracker_TestHelper_Test::DatabaseTrackerQuotaIntegration(false);
}

TEST(DatabaseTrackerTest, DatabaseTrackerQuotaIntegrationIncognitoMode) {
  DatabaseTracker_TestHelper_Test::DatabaseTrackerQuotaIntegration(true);
}

TEST(DatabaseTrackerTest, EmptyDatabaseNameIsValid) {
  DatabaseTracker_TestHelper_Test::EmptyDatabaseNameIsValid();
}

TEST(DatabaseTrackerTest, HandleSqliteError) {
  DatabaseTracker_TestHelper_Test::HandleSqliteError();
}

}  // namespace storage
