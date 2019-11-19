// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/local_storage_context_mojo.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_types.h"
#include "content/browser/dom_storage/test/storage_area_test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace content {

namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string Uint8VectorToStdString(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

void GetStorageUsageCallback(const base::RepeatingClosure& callback,
                             std::vector<StorageUsageInfo>* out_result,
                             std::vector<StorageUsageInfo> result) {
  *out_result = std::move(result);
  callback.Run();
}

class TestLevelDBObserver : public blink::mojom::StorageAreaObserver {
 public:
  struct Observation {
    enum { kAdd, kChange, kDelete, kDeleteAll } type;
    std::string key;
    std::string old_value;
    std::string new_value;
    std::string source;
  };

  TestLevelDBObserver() = default;

  mojo::PendingAssociatedRemote<blink::mojom::StorageAreaObserver> Bind() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  const std::vector<Observation>& observations() { return observations_; }

 private:
  void KeyAdded(const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& value,
                const std::string& source) override {
    observations_.push_back({Observation::kAdd, Uint8VectorToStdString(key), "",
                             Uint8VectorToStdString(value), source});
  }
  void KeyChanged(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& new_value,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kChange, Uint8VectorToStdString(key),
                             Uint8VectorToStdString(old_value),
                             Uint8VectorToStdString(new_value), source});
  }
  void KeyDeleted(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kDelete, Uint8VectorToStdString(key),
                             Uint8VectorToStdString(old_value), "", source});
  }
  void AllDeleted(const std::string& source) override {
    observations_.push_back({Observation::kDeleteAll, "", "", "", source});
  }
  void ShouldSendOldValueOnMutations(bool value) override {}

  std::vector<Observation> observations_;
  mojo::AssociatedReceiver<blink::mojom::StorageAreaObserver> receiver_{this};
};

}  // namespace

class LocalStorageContextMojoTest : public testing::Test {
 public:
  LocalStorageContextMojoTest()
      : mock_special_storage_policy_(new MockSpecialStoragePolicy()) {
    EXPECT_TRUE(temp_path_.CreateUniqueTempDir());
  }

  ~LocalStorageContextMojoTest() override {
    if (context_)
      ShutdownContext();

    // Ensure any opened database is really closed before attempting to delete
    // its storage path.
    RunUntilIdle();

    EXPECT_TRUE(temp_path_.Delete());
  }

  const base::FilePath& storage_path() const { return temp_path_.GetPath(); }

  LocalStorageContextMojo* context() {
    if (!context_) {
      context_ = new LocalStorageContextMojo(
          storage_path(), base::ThreadTaskRunnerHandle::Get(), task_runner_,
          special_storage_policy());
    }

    return context_;
  }

  void ShutdownContext() {
    context_->ShutdownAndDelete();
    context_ = nullptr;
    RunUntilIdle();
  }

  MockSpecialStoragePolicy* special_storage_policy() {
    return mock_special_storage_policy_.get();
  }

  void WaitForDatabaseOpen() {
    base::RunLoop loop;
    context()->SetDatabaseOpenCallbackForTesting(loop.QuitClosure());
    loop.Run();
  }

  void SetDatabaseEntry(base::StringPiece key, base::StringPiece value) {
    WaitForDatabaseOpen();
    base::RunLoop loop;
    context()->GetDatabaseForTesting().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          leveldb::Status status =
              db.Put(base::as_bytes(base::make_span(key)),
                     base::as_bytes(base::make_span(value)));
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();
  }

  void ClearDatabase() {
    WaitForDatabaseOpen();
    base::RunLoop loop;
    context()->GetDatabaseForTesting().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          leveldb::WriteBatch batch;
          leveldb::Status status = db.DeletePrefixed({}, &batch);
          ASSERT_TRUE(status.ok());
          status = db.Commit(&batch);
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();
  }

  std::map<std::string, std::string> GetDatabaseContents() {
    std::vector<storage::DomStorageDatabase::KeyValuePair> entries;
    WaitForDatabaseOpen();
    base::RunLoop loop;
    context()->GetDatabaseForTesting().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          leveldb::Status status = db.GetPrefixed({}, &entries);
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    std::map<std::string, std::string> contents;
    for (auto& entry : entries) {
      contents.emplace(std::string(entry.key.begin(), entry.key.end()),
                       std::string(entry.value.begin(), entry.value.end()));
    }

    return contents;
  }

  std::vector<StorageUsageInfo> GetStorageUsageSync() {
    base::RunLoop run_loop;
    std::vector<StorageUsageInfo> result;
    context()->GetStorageUsage(base::BindOnce(&GetStorageUsageCallback,
                                              run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  base::Optional<std::vector<uint8_t>> DoTestGet(
      const std::vector<uint8_t>& key) {
    const url::Origin kOrigin = url::Origin::Create(GURL("http://foobar.com"));
    mojo::Remote<blink::mojom::StorageArea> area;
    mojo::Remote<blink::mojom::StorageArea>
        dummy_area;  // To make sure values are cached.
    context()->OpenLocalStorage(kOrigin, area.BindNewPipeAndPassReceiver());
    context()->OpenLocalStorage(kOrigin,
                                dummy_area.BindNewPipeAndPassReceiver());
    std::vector<uint8_t> result;
    bool success = test::GetSync(area.get(), key, &result);
    return success ? base::Optional<std::vector<uint8_t>>(result)
                   : base::nullopt;
  }

  // Pumps both the main-thread sequence and the background database sequence
  // until both are idle.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void DoTestPut(LocalStorageContextMojo* context,
                 const std::vector<uint8_t>& key,
                 const std::vector<uint8_t>& value) {
    mojo::Remote<blink::mojom::StorageArea> area;
    bool success = false;
    base::RunLoop run_loop;
    context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
    area->Put(key, value, base::nullopt, "source",
              test::MakeSuccessCallback(run_loop.QuitClosure(), &success));
    run_loop.Run();
    EXPECT_TRUE(success);
    area.reset();
    RunUntilIdle();
  }

  bool DoTestGet(LocalStorageContextMojo* context,
                 const std::vector<uint8_t>& key,
                 std::vector<uint8_t>* result) {
    mojo::Remote<blink::mojom::StorageArea> area;
    context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    std::vector<blink::mojom::KeyValuePtr> data;
    bool success = false;
    bool done = false;
    area->GetAll(
        test::GetAllCallback::CreateAndBind(&done, run_loop.QuitClosure()),
        test::MakeGetAllCallback(&success, &data));
    run_loop.Run();
    EXPECT_TRUE(done);
    EXPECT_TRUE(success);

    for (auto& entry : data) {
      if (key == entry->key) {
        *result = std::move(entry->value);
        return true;
      }
    }
    result->clear();
    return false;
  }

  base::FilePath FirstEntryInDir() {
    base::FileEnumerator enumerator(
        storage_path(), false /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    return enumerator.Next();
  }

 private:
  // testing::Test:
  void TearDown() override {
    // Some of these tests close message pipes which serve as master interfaces
    // to other associated interfaces; this in turn schedules tasks to invoke
    // the associated interfaces' error handlers, and local storage code relies
    // on those handlers running in order to avoid memory leaks at shutdown.
    RunUntilIdle();
  }

  BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_{
      base::ThreadTaskRunnerHandle::Get()};

  LocalStorageContextMojo* context_ = nullptr;

  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageContextMojoTest);
};

TEST_F(LocalStorageContextMojoTest, Basic) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());

  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  RunUntilIdle();

  // Should have three rows of data, one for the version, one for the actual
  // data and one for metadata.
  EXPECT_EQ(3u, GetDatabaseContents().size());
}

TEST_F(LocalStorageContextMojoTest, OriginsAreIndependent) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com:123"));
  url::Origin origin2 = url::Origin::Create(GURL("http://foobar.com:1234"));
  auto key1 = StdStringToUint8Vector("4key");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key1, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  RunUntilIdle();
  EXPECT_EQ(5u, GetDatabaseContents().size());
}

TEST_F(LocalStorageContextMojoTest, WrapperOutlivesMojoConnection) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Write some data to the DB.
  mojo::Remote<blink::mojom::StorageArea> area;
  mojo::Remote<blink::mojom::StorageArea>
      dummy_area;  // To make sure values are cached.
  const url::Origin kOrigin(url::Origin::Create(GURL("http://foobar.com")));
  context()->OpenLocalStorage(kOrigin, area.BindNewPipeAndPassReceiver());
  context()->OpenLocalStorage(kOrigin, dummy_area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());

  area.reset();
  dummy_area.reset();
  RunUntilIdle();

  // Clear all the data from the backing database.
  EXPECT_FALSE(GetDatabaseContents().empty());
  ClearDatabase();

  // Data should still be readable, because despite closing the area
  // connection above, the actual area instance should have been kept alive.
  EXPECT_EQ(value, DoTestGet(key));

  // Now purge memory.
  context()->PurgeMemory();

  // And make sure caches were actually cleared.
  EXPECT_EQ(base::nullopt, DoTestGet(key));
}

TEST_F(LocalStorageContextMojoTest, OpeningWrappersPurgesInactiveWrappers) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Write some data to the DB.
  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());

  area.reset();
  RunUntilIdle();

  // Clear all the data from the backing database.
  EXPECT_FALSE(GetDatabaseContents().empty());
  ClearDatabase();

  // Now open many new areas (for different origins) to trigger clean up.
  for (int i = 1; i <= 100; ++i) {
    context()->OpenLocalStorage(url::Origin::Create(GURL(base::StringPrintf(
                                    "http://example.com:%d", i))),
                                area.BindNewPipeAndPassReceiver());
    area.reset();
  }

  RunUntilIdle();

  // And make sure caches were actually cleared.
  EXPECT_EQ(base::nullopt, DoTestGet(key));
}

TEST_F(LocalStorageContextMojoTest, ValidVersion) {
  SetDatabaseEntry("VERSION", "1");
  SetDatabaseEntry(std::string("_http://foobar.com") + '\x00' + "key", "value");
  ShutdownContext();

  EXPECT_EQ(StdStringToUint8Vector("value"),
            DoTestGet(StdStringToUint8Vector("key")));
}

TEST_F(LocalStorageContextMojoTest, InvalidVersion) {
  SetDatabaseEntry("VERSION", "foobar");
  SetDatabaseEntry(std::string("_http://foobar.com") + '\x00' + "key", "value");

  // Force the context to reload the database, which should fail due to invalid
  // version data.
  ShutdownContext();

  EXPECT_EQ(base::nullopt, DoTestGet(StdStringToUint8Vector("key")));
}

TEST_F(LocalStorageContextMojoTest, VersionOnlyWrittenOnCommit) {
  EXPECT_EQ(base::nullopt, DoTestGet(StdStringToUint8Vector("key")));

  RunUntilIdle();
  EXPECT_TRUE(GetDatabaseContents().empty());
}

TEST_F(LocalStorageContextMojoTest, GetStorageUsage_NoData) {
  std::vector<StorageUsageInfo> info = GetStorageUsageSync();
  EXPECT_EQ(0u, info.size());
}

TEST_F(LocalStorageContextMojoTest, GetStorageUsage_Data) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key1 = StdStringToUint8Vector("key1");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  base::Time before_write = base::Time::Now();

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key1, value, base::nullopt, "source", base::DoNothing());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();

  base::Time after_write = base::Time::Now();

  std::vector<StorageUsageInfo> info = GetStorageUsageSync();
  ASSERT_EQ(2u, info.size());
  if (info[0].origin == origin2)
    std::swap(info[0], info[1]);
  EXPECT_EQ(origin1, info[0].origin);
  EXPECT_EQ(origin2, info[1].origin);
  EXPECT_LE(before_write, info[0].last_modified);
  EXPECT_LE(before_write, info[1].last_modified);
  EXPECT_GE(after_write, info[0].last_modified);
  EXPECT_GE(after_write, info[1].last_modified);
  EXPECT_GT(info[0].total_size_bytes, info[1].total_size_bytes);
}

TEST_F(LocalStorageContextMojoTest, MetaDataClearedOnDelete) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Delete(key, value, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(3u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos, entry.first.find(origin1.Serialize()));
    EXPECT_NE(std::string::npos, entry.first.find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, MetaDataClearedOnDeleteAll) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->DeleteAll("source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(3u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos, entry.first.find(origin1.Serialize()));
    EXPECT_NE(std::string::npos, entry.first.find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, DeleteStorage) {
  SetDatabaseEntry("VERSION", "1");
  SetDatabaseEntry(std::string("_http://foobar.com") + '\x00' + "key", "value");
  ShutdownContext();

  base::RunLoop run_loop;
  context()->DeleteStorage(url::Origin::Create(GURL("http://foobar.com")),
                           run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1u, GetDatabaseContents().size());
}

TEST_F(LocalStorageContextMojoTest, DeleteStorageWithoutConnection) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();
  EXPECT_FALSE(GetDatabaseContents().empty());

  context()->DeleteStorage(origin1, base::DoNothing());
  RunUntilIdle();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(3u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos, entry.first.find(origin1.Serialize()));
    EXPECT_NE(std::string::npos, entry.first.find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, DeleteStorageNotifiesWrapper) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();
  EXPECT_FALSE(GetDatabaseContents().empty());

  TestLevelDBObserver observer;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  RunUntilIdle();

  context()->DeleteStorage(origin1, base::DoNothing());
  RunUntilIdle();

  ASSERT_EQ(1u, observer.observations().size());
  EXPECT_EQ(TestLevelDBObserver::Observation::kDeleteAll,
            observer.observations()[0].type);

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(3u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos, entry.first.find(origin1.Serialize()));
    EXPECT_NE(std::string::npos, entry.first.find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, DeleteStorageWithPendingWrites) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  RunUntilIdle();
  EXPECT_FALSE(GetDatabaseContents().empty());

  TestLevelDBObserver observer;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  area->Put(StdStringToUint8Vector("key2"), value, base::nullopt, "source",
            base::DoNothing());
  RunUntilIdle();

  context()->DeleteStorage(origin1, base::DoNothing());
  RunUntilIdle();

  ASSERT_EQ(2u, observer.observations().size());
  EXPECT_EQ(TestLevelDBObserver::Observation::kAdd,
            observer.observations()[0].type);
  EXPECT_EQ(TestLevelDBObserver::Observation::kDeleteAll,
            observer.observations()[1].type);

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(3u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos, entry.first.find(origin1.Serialize()));
    EXPECT_NE(std::string::npos, entry.first.find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, Migration) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  base::string16 key = base::ASCIIToUTF16("key");
  base::string16 value = base::ASCIIToUTF16("value");
  base::string16 key2 = base::ASCIIToUTF16("key2");
  key2.push_back(0xd83d);
  key2.push_back(0xde00);

  // We want to populate the Local Storage directory before the implementation
  // has created it, so we have to create it ourselves here.
  const base::FilePath local_storage_path =
      storage_path().Append(storage::kLocalStoragePath);
  ASSERT_TRUE(base::CreateDirectory(local_storage_path));

  const base::FilePath old_db_path = local_storage_path.Append(
      LocalStorageContextMojo::LegacyDatabaseFileNameFromOrigin(origin1));
  {
    DOMStorageDatabase db(old_db_path);
    DOMStorageValuesMap data;
    data[key] = base::NullableString16(value, false);
    data[key2] = base::NullableString16(value, false);
    db.CommitChanges(false, data);
  }
  EXPECT_TRUE(base::PathExists(old_db_path));

  // Opening origin2 and accessing its data should not migrate anything.
  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());

  // To make sure values are cached.
  mojo::Remote<blink::mojom::StorageArea> dummy_area;
  context()->OpenLocalStorage(origin2, dummy_area.BindNewPipeAndPassReceiver());

  area->Get(std::vector<uint8_t>(), base::DoNothing());
  area.reset();
  dummy_area.reset();
  RunUntilIdle();

  EXPECT_TRUE(GetDatabaseContents().empty());

  // Opening origin1 and accessing its data should migrate its storage.
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  context()->OpenLocalStorage(origin1, dummy_area.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  area->Get(std::vector<uint8_t>(),
            base::BindLambdaForTesting(
                [&](bool, const std::vector<uint8_t>&) { loop.Quit(); }));
  loop.Run();

  EXPECT_FALSE(GetDatabaseContents().empty());

  {
    std::vector<uint8_t> result;
    bool success = test::GetSync(
        area.get(), LocalStorageContextMojo::MigrateString(key), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(LocalStorageContextMojo::MigrateString(value), result);
  }

  {
    std::vector<uint8_t> result;
    bool success = test::GetSync(
        area.get(), LocalStorageContextMojo::MigrateString(key2), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(LocalStorageContextMojo::MigrateString(value), result);
  }

  // Origin1 should no longer exist in old storage.
  EXPECT_FALSE(base::PathExists(old_db_path));
}

static std::string EncodeKeyAsUTF16(const std::string& origin,
                                    const base::string16& key) {
  std::string result = '_' + origin + '\x00' + '\x00';
  std::copy(reinterpret_cast<const char*>(key.data()),
            reinterpret_cast<const char*>(key.data()) +
                key.size() * sizeof(base::char16),
            std::back_inserter(result));
  return result;
}

TEST_F(LocalStorageContextMojoTest, FixUp) {
  SetDatabaseEntry("VERSION", "1");
  // Add mock data for the "key" key, with both possible encodings for key.
  // We expect the value of the correctly encoded key to take precedence over
  // the incorrectly encoded key (and expect the incorrectly encoded key to be
  // deleted.
  SetDatabaseEntry(std::string("_http://foobar.com") + '\x00' + "\x01key",
                   "value1");
  SetDatabaseEntry(
      EncodeKeyAsUTF16("http://foobar.com", base::ASCIIToUTF16("key")),
      "value2");
  // Also add mock data for the "foo" key, this time only with the incorrec
  // encoding. This should be updated to the correct encoding.
  SetDatabaseEntry(
      EncodeKeyAsUTF16("http://foobar.com", base::ASCIIToUTF16("foo")),
      "value3");

  mojo::Remote<blink::mojom::StorageArea> area;
  mojo::Remote<blink::mojom::StorageArea>
      dummy_area;  // To make sure values are cached.
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              dummy_area.BindNewPipeAndPassReceiver());

  {
    std::vector<uint8_t> result;
    bool success =
        test::GetSync(area.get(), StdStringToUint8Vector("\x01key"), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(StdStringToUint8Vector("value1"), result);
  }
  {
    std::vector<uint8_t> result;
    bool success = test::GetSync(area.get(),
                                 StdStringToUint8Vector("\x01"
                                                        "foo"),
                                 &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(StdStringToUint8Vector("value3"), result);
  }

  // Expect 4 rows in the database: VERSION, meta-data for the origin, and two
  // rows of actual data.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(4u, contents.size());
  EXPECT_EQ("value1", contents.rbegin()->second);
  EXPECT_EQ("value3", std::next(contents.rbegin())->second);
}

TEST_F(LocalStorageContextMojoTest, ShutdownClearsData) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key1 = StdStringToUint8Vector("key1");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());

  area->Put(key1, value, base::nullopt, "source", base::DoNothing());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to the DB.
  RunUntilIdle();

  special_storage_policy()->AddSessionOnly(origin1.GetURL());
  ShutdownContext();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(3u, contents.size());
  for (const auto& entry : contents) {
    if (entry.first == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos, entry.first.find(origin1.Serialize()));
    EXPECT_NE(std::string::npos, entry.first.find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, InMemory) {
  auto* context = new LocalStorageContextMojo(
      base::FilePath(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area.BindNewPipeAndPassReceiver());
  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Should not have created any files.
  EXPECT_TRUE(FirstEntryInDir().empty());

  // Re-opening should get fresh data.
  context = new LocalStorageContextMojo(
      base::FilePath(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  EXPECT_FALSE(DoTestGet(context, key, &result));
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, InMemoryInvalidPath) {
  auto* context = new LocalStorageContextMojo(
      base::FilePath(FILE_PATH_LITERAL("../../")),
      base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area.BindNewPipeAndPassReceiver());

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Should not have created any files.
  EXPECT_TRUE(FirstEntryInDir().empty());
}

TEST_F(LocalStorageContextMojoTest, OnDisk) {
  auto* context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Should have created files.
  EXPECT_EQ(base::FilePath(storage::kLocalStoragePath),
            FirstEntryInDir().BaseName());

  // Should be able to re-open.
  context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, InvalidVersionOnDisk) {
  // Create context and add some data to it.
  auto* context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  {
    // Mess up version number in database.
    leveldb_env::ChromiumEnv env;
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.env = &env;
    base::FilePath db_path =
        storage_path()
            .Append(storage::kLocalStoragePath)
            .AppendASCII(storage::kLocalStorageLeveldbName);
    ASSERT_TRUE(leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db).ok());
    ASSERT_TRUE(db->Put(leveldb::WriteOptions(), "VERSION", "argh").ok());
  }

  // Make sure data is gone.
  context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  EXPECT_FALSE(DoTestGet(context, key, &result));

  // Write data again.
  DoTestPut(context, key, value);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Data should have been preserved now.
  context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, CorruptionOnDisk) {
  // Create context and add some data to it.
  auto* context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Delete manifest files to mess up opening DB.
  base::FilePath db_path = storage_path()
                               .Append(storage::kLocalStoragePath)
                               .AppendASCII(storage::kLocalStorageLeveldbName);
  base::FileEnumerator file_enum(db_path, true, base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("MANIFEST*"));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::DeleteFile(name, false);
  }

  // Make sure data is gone.
  context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  EXPECT_FALSE(DoTestGet(context, key, &result));

  // Write data again.
  DoTestPut(context, key, value);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Data should have been preserved now.
  context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, RecreateOnCommitFailure) {
  auto* context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);

  base::Optional<base::RunLoop> open_loop;
  base::Optional<base::RunLoop> destruction_loop;
  size_t num_database_open_requests = 0;
  context->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));

  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  open_loop.emplace();

  // Open three connections to the database. Two to the same origin, and a third
  // to a different origin.
  mojo::Remote<blink::mojom::StorageArea> area1;
  mojo::Remote<blink::mojom::StorageArea> area2;
  mojo::Remote<blink::mojom::StorageArea> area3;

  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area1.BindNewPipeAndPassReceiver());
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area2.BindNewPipeAndPassReceiver());
  context->OpenLocalStorage(url::Origin::Create(GURL("http://example.com")),
                            area3.BindNewPipeAndPassReceiver());
  open_loop->Run();

  // Add observers to the first two connections.
  TestLevelDBObserver observer1;
  area1->AddObserver(observer1.Bind());
  TestLevelDBObserver observer2;
  area2->AddObserver(observer2.Bind());

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // This loop will be Quit if and when the current database instance is
  // destroyed, which should happen after many commit failures.
  destruction_loop.emplace();

  bool first_database_destroyed = false;
  context->GetDatabaseForTesting().PostTaskWithThisObject(
      FROM_HERE,
      base::BindLambdaForTesting([&](storage::DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(base::BindLambdaForTesting([&] {
          first_database_destroyed = true;
          destruction_loop->Quit();
        }));
      }));

  // Also prepare for another database connection, this time without making
  // commits fail.
  open_loop.emplace();
  num_database_open_requests = 0;
  context->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first origin. This put operation should result in a
  // pending commit that will get cancelled when the database is destroyed.
  area3->Put(key, value, base::nullopt, "source",
             base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  size_t values_written = 0;
  while (area1.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    value[0]++;
    area1->Put(key, value, base::nullopt, "source",
               base::BindLambdaForTesting([&](bool success) {
                 EXPECT_TRUE(success);
                 values_written++;
               }));
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context->FlushOriginForTesting(
        url::Origin::Create(GURL("http://foobar.com")));
  }
  area1.reset();

  // Wait for LocalStorageContextMojo to try to reconnect to the database, and
  // Enough commit failures should happen during this loop to cause the database
  // to be destroyed.
  destruction_loop->Run();
  EXPECT_TRUE(first_database_destroyed);

  // The connection to the second area should end up closed as well.
  area2.FlushForTesting();
  EXPECT_FALSE(area2.is_connected());

  // Reconnect |area1| to the database, and try to read a value.
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area1.BindNewPipeAndPassReceiver());
  base::RunLoop delete_loop;
  bool success = true;
  TestLevelDBObserver observer3;
  area1->AddObserver(observer3.Bind());
  area1->Delete(key, base::nullopt, "source",
                base::BindLambdaForTesting([&](bool success_in) {
                  success = success_in;
                  delete_loop.Quit();
                }));

  // The new database should be ready to go.
  open_loop->Run();
  ASSERT_EQ(1u, num_database_open_requests);

  // And deleting the value from the new area should have failed (as the
  // database is empty).
  delete_loop.Run();
  EXPECT_EQ(0u, observer3.observations().size());
  area1.reset();

  {
    // Committing data should now work.
    DoTestPut(context, key, value);
    std::vector<uint8_t> result;
    EXPECT_TRUE(DoTestGet(context, key, &result));
    EXPECT_EQ(value, result);
  }

  // Observers should have seen one Add event and a number of Change events for
  // all commits until the connection was closed.
  ASSERT_EQ(values_written, observer2.observations().size());
  for (size_t i = 0; i < values_written; ++i) {
    EXPECT_EQ(i ? TestLevelDBObserver::Observation::kChange
                : TestLevelDBObserver::Observation::kAdd,
              observer2.observations()[i].type);
    EXPECT_EQ(Uint8VectorToStdString(key), observer2.observations()[i].key);
  }

  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, DontRecreateOnRepeatedCommitFailure) {
  auto* context = new LocalStorageContextMojo(
      storage_path(), base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);

  // Ensure that the opened database always fails on write.
  base::Optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  context->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));
  open_loop.emplace();

  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Open a connection to the database.
  mojo::Remote<blink::mojom::StorageArea> area;
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area.BindNewPipeAndPassReceiver());
  open_loop->Run();

  // Ensure that all commits fail on the database, and that we observe its
  // destruction.
  context->GetDatabaseForTesting().PostTaskWithThisObject(
      FROM_HERE,
      base::BindLambdaForTesting([&](storage::DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until LocalStorageContextMojo tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();

  // Repeatedly write data to the database, to trigger enough commit errors.
  base::Optional<std::vector<uint8_t>> old_value;
  while (area.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    value[0]++;
    area->Put(key, value, old_value, "source",
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
              }));
    old_value = std::vector<uint8_t>(value);
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context->FlushOriginForTesting(
        url::Origin::Create(GURL("http://foobar.com")));
  }
  area.reset();

  // Wait for LocalStorageContextMojo to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  context->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));
  open_loop->Run();
  EXPECT_EQ(2u, num_database_open_requests);
  EXPECT_EQ(1u, num_databases_destroyed);
  context->GetDatabaseForTesting().PostTaskWithThisObject(
      FROM_HERE, base::BindOnce([](storage::DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
      }));

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area.BindNewPipeAndPassReceiver());
  old_value = base::nullopt;
  for (int i = 0; i < 64; ++i) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    value[0]++;
    area->Put(key, value, old_value, "source",
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
              }));
    RunUntilIdle();
    old_value = value;
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context->FlushOriginForTesting(
        url::Origin::Create(GURL("http://foobar.com")));
  }

  // Should still be connected after all that.
  RunUntilIdle();
  EXPECT_TRUE(area.is_connected());

  context->ShutdownAndDelete();
}

}  // namespace content
