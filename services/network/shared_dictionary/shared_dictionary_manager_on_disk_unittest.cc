// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/http/http_response_headers.h"
#include "net/shared_dictionary/shared_dictionary.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/shared_dictionary_error.mojom.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::ElementsAre;
using ::testing::Pair;

namespace network {

namespace {

const GURL kUrl("https://origin.test/");
const net::SchemefulSite kSite(kUrl);
const std::string kTestData1 = "Hello world";
const std::string kTestData2 = "Bonjour le monde";

// Default cache control header for dictionary entries which expires in 30 days.
const std::string kDefaultCacheControlHeader =
    "cache-control: max-age=2592000\n";

const int kCurrentVersionNumber = 1;

base::OnceCallback<bool()> DummyAccessAllowedCheckCallback() {
  return base::BindOnce([]() { return true; });
}

void WriteDictionary(SharedDictionaryStorage* storage,
                     const GURL& dictionary_url,
                     const std::string& match,
                     const std::string& data) {
  const std::string use_as_dictionary_header =
      base::StrCat({"match=\"/", match, "\""});
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": ", use_as_dictionary_header, "\n", kDefaultCacheControlHeader,
           "\n"}));
  ASSERT_TRUE(headers);
  base::expected<scoped_refptr<SharedDictionaryWriter>,
                 mojom::SharedDictionaryError>
      writer = SharedDictionaryStorage::MaybeCreateWriter(
          use_as_dictionary_header, /*shared_dictionary_writer_enabled=*/true,
          storage, mojom::RequestMode::kSameOrigin,
          mojom::FetchResponseType::kBasic, dictionary_url,
          /*request_time=*/base::Time::Now(),
          /*response_time=*/base::Time::Now(), *headers,
          /*was_fetched_via_cache=*/false, DummyAccessAllowedCheckCallback());
  ASSERT_TRUE(writer.has_value());
  ASSERT_TRUE(*writer);
  (*writer)->Append(data.c_str(), data.size());
  (*writer)->Finish();
}
void WriteDictionaryWithExpiry(SharedDictionaryStorage* storage,
                               const GURL& dictionary_url,
                               const std::string& match,
                               const base::TimeDelta& expires,
                               const std::string& data) {
  const std::string use_as_dictionary_header =
      base::StrCat({"match=\"/", match, "\""});
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ":", use_as_dictionary_header, "\n", "cache-control: max-age=",
           base::NumberToString(expires.InSeconds()), "\n\n"}));
  ASSERT_TRUE(headers);
  base::expected<scoped_refptr<SharedDictionaryWriter>,
                 mojom::SharedDictionaryError>
      writer = SharedDictionaryStorage::MaybeCreateWriter(
          use_as_dictionary_header, /*shared_dictionary_writer_enabled=*/true,
          storage, mojom::RequestMode::kSameOrigin,
          mojom::FetchResponseType::kBasic, dictionary_url,
          /*request_time=*/base::Time::Now(),
          /*response_time=*/base::Time::Now(), *headers,
          /*was_fetched_via_cache=*/false, DummyAccessAllowedCheckCallback());
  ASSERT_TRUE(writer.has_value());
  ASSERT_TRUE(*writer);
  (*writer)->Append(data.c_str(), data.size());
  (*writer)->Finish();
}

bool DiskCacheEntryExists(SharedDictionaryManager* manager,
                          const std::string& disk_cache_key) {
  TestEntryResultCompletionCallback open_callback;
  disk_cache::EntryResult open_result = open_callback.GetResult(
      static_cast<SharedDictionaryManagerOnDisk*>(manager)
          ->disk_cache()
          .OpenOrCreateEntry(disk_cache_key,
                             /*create=*/false, open_callback.callback()));
  return open_result.net_error() == net::OK;
}

bool DiskCacheEntryExists(SharedDictionaryManager* manager,
                          const base::UnguessableToken& disk_cache_key_token) {
  return DiskCacheEntryExists(manager, disk_cache_key_token.ToString());
}

void DoomDiskCacheEntry(SharedDictionaryManager* manager,
                        const base::UnguessableToken& disk_cache_key_token) {
  net::TestCompletionCallback doom_callback;
  EXPECT_EQ(net::OK, doom_callback.GetResult(
                         static_cast<SharedDictionaryManagerOnDisk*>(manager)
                             ->disk_cache()
                             .DoomEntry(disk_cache_key_token.ToString(),
                                        doom_callback.callback())));
}

void WriteDiskCacheEntry(SharedDictionaryManager* manager,
                         const std::string& disk_cache_key,
                         const std::string& data) {
  SharedDictionaryDiskCache& disk_cache =
      static_cast<SharedDictionaryManagerOnDisk*>(manager)->disk_cache();

  TestEntryResultCompletionCallback create_callback;

  // Create the entry.
  disk_cache::EntryResult create_result =
      create_callback.GetResult(disk_cache.OpenOrCreateEntry(
          disk_cache_key, /*create=*/true, create_callback.callback()));
  EXPECT_EQ(net::OK, create_result.net_error());
  disk_cache::ScopedEntryPtr created_entry;
  created_entry.reset(create_result.ReleaseEntry());
  ASSERT_TRUE(created_entry);

  // Write to the entry.
  scoped_refptr<net::StringIOBuffer> write_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(data);
  net::TestCompletionCallback write_callback;
  EXPECT_EQ(
      base::checked_cast<int>(data.size()),
      write_callback.GetResult(created_entry->WriteData(
          /*index=*/1, /*offset=*/0, write_buffer.get(), write_buffer->size(),
          write_callback.callback(), /*truncate=*/false)));
}

}  // namespace

class SharedDictionaryManagerOnDiskTest : public ::testing::Test {
 public:
  SharedDictionaryManagerOnDiskTest() = default;
  ~SharedDictionaryManagerOnDiskTest() override = default;

  SharedDictionaryManagerOnDiskTest(const SharedDictionaryManagerOnDiskTest&) =
      delete;
  SharedDictionaryManagerOnDiskTest& operator=(
      const SharedDictionaryManagerOnDiskTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(tmp_directory_.CreateUniqueTempDir());
    database_path_ = tmp_directory_.GetPath().Append(FILE_PATH_LITERAL("db"));
    cache_directory_path_ =
        tmp_directory_.GetPath().Append(FILE_PATH_LITERAL("cache"));
  }
  void TearDown() override { FlushCacheTasks(); }

 protected:
  static base::UnguessableToken GetDiskCacheKeyTokenOfFirstDictionary(
      const std::map<
          url::SchemeHostPort,
          std::map<std::tuple<std::string, std::set<mojom::RequestDestination>>,
                   SharedDictionaryStorageOnDisk::WrappedDictionaryInfo>>&
          dictionary_map,
      const std::string& scheme_host_port_str) {
    auto it =
        dictionary_map.find(url::SchemeHostPort(GURL(scheme_host_port_str)));
    CHECK(it != dictionary_map.end()) << scheme_host_port_str;
    CHECK(!it->second.empty());
    return it->second.begin()->second.disk_cache_key_token();
  }

  std::unique_ptr<SharedDictionaryManager> CreateSharedDictionaryManager(
      uint64_t cache_max_size = 0,
      uint64_t cache_max_count =
          shared_dictionary::kDictionaryMaxCountPerNetworkContext) {
    return SharedDictionaryManager::CreateOnDisk(
        database_path_, cache_directory_path_, cache_max_size, cache_max_count,
#if BUILDFLAG(IS_ANDROID)
        disk_cache::ApplicationStatusListenerGetter(),
#endif  // BUILDFLAG(IS_ANDROID)
        /*file_operations_factory=*/nullptr);
  }
  const std::map<
      url::SchemeHostPort,
      std::map<std::tuple<std::string, std::set<mojom::RequestDestination>>,
               SharedDictionaryStorageOnDisk::WrappedDictionaryInfo>>&
  GetOnDiskDictionaryMap(SharedDictionaryStorage* storage) {
    return static_cast<SharedDictionaryStorageOnDisk*>(storage)
        ->GetDictionaryMapForTesting();
  }
  void FlushCacheTasks() {
    disk_cache::FlushCacheThreadForTesting();
    task_environment_.RunUntilIdle();
  }
  void CorruptDiskCache() {
    // Corrupt the fake index file for the populated simple cache.
    const base::FilePath index_file_path =
        cache_directory_path_.Append(FILE_PATH_LITERAL("index"));
    ASSERT_TRUE(base::WriteFile(index_file_path, "corrupted"));
    file_permissions_restorer_ = std::make_unique<base::FilePermissionRestorer>(
        tmp_directory_.GetPath());
    // Mark the parent directory unwritable, so that we can't restore the dist
    ASSERT_TRUE(base::MakeFileUnwritable(tmp_directory_.GetPath()));
  }

  void CorruptDatabase() {
    CHECK(sql::test::CorruptSizeInHeader(database_path_));
  }

  void ManipulateDatabase(const std::vector<std::string>& queries) {
    std::unique_ptr<sql::Database> db =
        std::make_unique<sql::Database>(sql::DatabaseOptions{});
    ASSERT_TRUE(db->Open(database_path_));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(db.get(), kCurrentVersionNumber,
                                kCurrentVersionNumber));
    for (const std::string& query : queries) {
      ASSERT_TRUE(db->Execute(query));
    }
    db->Close();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::ScopedTempDir tmp_directory_;
  base::FilePath database_path_;
  base::FilePath cache_directory_path_;
  // `file_permissions_restorer_` must be below `tmp_directory_` to restore the
  // file permission correctly.
  std::unique_ptr<base::FilePermissionRestorer> file_permissions_restorer_;
};

TEST_F(SharedDictionaryManagerOnDiskTest, ReusingRefCountedSharedDictionary) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  WriteDictionary(storage.get(), GURL("https://origin.test/dict"), "testfile*",
                  kTestData1);

  FlushCacheTasks();

  // Check the returned dictionary from GetDictionarySync().
  scoped_refptr<net::SharedDictionary> dict1 =
      storage->GetDictionarySync(GURL("https://origin.test/testfile?1"),
                                 mojom::RequestDestination::kEmpty);
  ASSERT_TRUE(dict1);
  {
    base::RunLoop run_loop;
    EXPECT_EQ(net::ERR_IO_PENDING,
              dict1->ReadAll(base::BindLambdaForTesting([&](int rv) {
                EXPECT_EQ(net::OK, rv);
                run_loop.Quit();
              })));
    run_loop.Run();
  }
  scoped_refptr<net::SharedDictionary> dict2 =
      storage->GetDictionarySync(GURL("https://origin.test/testfile?2"),
                                 mojom::RequestDestination::kEmpty);
  ASSERT_TRUE(dict2);
  // `dict2` shares the same RefCountedSharedDictionary with `dict1`. So
  // ReadAll() must synchronously return OK.
  EXPECT_EQ(net::OK, dict2->ReadAll(base::BindLambdaForTesting(
                         [&](int rv) { NOTREACHED_IN_MIGRATION(); })));
  // `dict2` shares the same IOBuffer with `dict1`.
  EXPECT_EQ(dict1->data(), dict2->data());
  EXPECT_EQ(dict1->size(), dict2->size());
  EXPECT_EQ(dict1->hash(), dict2->hash());
  EXPECT_EQ(kTestData1,
            std::string(reinterpret_cast<const char*>(dict1->data()->data()),
                        dict1->size()));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       MaybeCreateWriterAfterManagerDeleted) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  manager.reset();

  const std::string use_as_dictionary_header =
      base::StrCat({"match=\"/testfile*\""});
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": ", use_as_dictionary_header, "\n", kDefaultCacheControlHeader,
           "\n"}));
  ASSERT_TRUE(headers);

  // MaybeCreateWriter() must return nullptr, after `manager` was deleted.
  base::expected<scoped_refptr<SharedDictionaryWriter>,
                 mojom::SharedDictionaryError>
      writer = SharedDictionaryStorage::MaybeCreateWriter(
          use_as_dictionary_header, /*shared_dictionary_writer_enabled=*/true,
          storage.get(), mojom::RequestMode::kSameOrigin,
          mojom::FetchResponseType::kBasic, GURL("https://origin.test/dict"),
          /*request_time=*/base::Time::Now(),
          /*response_time=*/base::Time::Now(), *headers,
          /*was_fetched_via_cache=*/false, DummyAccessAllowedCheckCallback());
  EXPECT_FALSE(writer.has_value());
  EXPECT_EQ(mojom::SharedDictionaryError::kWriteErrorShuttingDown,
            writer.error());
}

TEST_F(SharedDictionaryManagerOnDiskTest, GetDictionaryAfterManagerDeleted) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  manager.reset();

  // GetDictionarySync() must return nullptr, after `manager` was deleted.
  scoped_refptr<net::SharedDictionary> dict =
      storage->GetDictionarySync(GURL("https://origin.test/testfile?1"),
                                 mojom::RequestDestination::kEmpty);
  EXPECT_FALSE(dict);
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       DictionaryWrittenInDiskCacheAfterManagerDeleted) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin.test/dict"), "testfile*",
                  kTestData1);
  // Test that deleting `manager` while writing the dictionary doesn't cause
  // crash.
  manager.reset();
  FlushCacheTasks();
}

TEST_F(SharedDictionaryManagerOnDiskTest, OverridingDictionary) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin.test/dict1"), "testfile*",
                  kTestData1);
  FlushCacheTasks();

  base::UnguessableToken disk_cache_key_token1;
  {
    const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
    ASSERT_EQ(1u, dictionary_map.size());
    ASSERT_EQ(1u, dictionary_map.begin()->second.size());
    disk_cache_key_token1 =
        dictionary_map.begin()->second.begin()->second.disk_cache_key_token();
  }

  // Check the returned dictionary from GetDictionarySync().
  scoped_refptr<net::SharedDictionary> dict1 = storage->GetDictionarySync(
      GURL("https://origin.test/testfile"), mojom::RequestDestination::kEmpty);
  ASSERT_TRUE(dict1);

  // The disk cache entry must exist.
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), disk_cache_key_token1));

  // Write different test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin.test/dict2"), "testfile*",
                  kTestData2);

  FlushCacheTasks();

  base::UnguessableToken disk_cache_key_token2;
  {
    const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
    ASSERT_EQ(1u, dictionary_map.size());
    ASSERT_EQ(1u, dictionary_map.begin()->second.size());
    disk_cache_key_token2 =
        dictionary_map.begin()->second.begin()->second.disk_cache_key_token();
  }

  EXPECT_NE(disk_cache_key_token1, disk_cache_key_token2);

  // The disk cache entry should have been doomed.
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), disk_cache_key_token1));

  scoped_refptr<net::SharedDictionary> dict2 = storage->GetDictionarySync(
      GURL("https://origin.test/testfile"), mojom::RequestDestination::kEmpty);
  ASSERT_TRUE(dict2);

  // We can read the new dictionary from `dict2`.
  net::TestCompletionCallback read_callback2;
  EXPECT_EQ(net::OK, read_callback2.GetResult(
                         dict2->ReadAll(read_callback2.callback())));
  EXPECT_EQ(kTestData2,
            std::string(reinterpret_cast<const char*>(dict2->data()->data()),
                        dict2->size()));

  // We can still read the old dictionary from `dict1`.
  net::TestCompletionCallback read_callback1;
  EXPECT_EQ(net::OK, read_callback1.GetResult(
                         dict1->ReadAll(read_callback1.callback())));
  EXPECT_EQ(kTestData1,
            std::string(reinterpret_cast<const char*>(dict1->data()->data()),
                        dict1->size()));
}

TEST_F(SharedDictionaryManagerOnDiskTest, MultipleDictionaries) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);

  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);

    // Write the test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin.test/dict1"),
                    "testfile1*", kTestData1);

    WriteDictionary(storage.get(), GURL("https://origin.test/dict2"),
                    "testfile2*", kTestData2);

    FlushCacheTasks();

    scoped_refptr<net::SharedDictionary> dict1 =
        storage->GetDictionarySync(GURL("https://origin.test/testfile1"),
                                   mojom::RequestDestination::kEmpty);
    ASSERT_TRUE(dict1);

    scoped_refptr<net::SharedDictionary> dict2 =
        storage->GetDictionarySync(GURL("https://origin.test/testfile2"),
                                   mojom::RequestDestination::kEmpty);
    ASSERT_TRUE(dict2);

    net::TestCompletionCallback read_callback1;
    EXPECT_EQ(net::OK, read_callback1.GetResult(
                           dict1->ReadAll(read_callback1.callback())));
    EXPECT_EQ(kTestData1,
              std::string(reinterpret_cast<const char*>(dict1->data()->data()),
                          dict1->size()));

    net::TestCompletionCallback read_callback2;
    EXPECT_EQ(net::OK, read_callback2.GetResult(
                           dict2->ReadAll(read_callback2.callback())));
    EXPECT_EQ(kTestData2,
              std::string(reinterpret_cast<const char*>(dict2->data()->data()),
                          dict2->size()));
    // Releasing `dict1`, `dict2`, `storage` and `manager`.
  }
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  // The dictionaries must be available after recreating `manager`.
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
  ASSERT_EQ(1u, dictionary_map.size());
  ASSERT_EQ(2u, dictionary_map.begin()->second.size());

  scoped_refptr<net::SharedDictionary> dict1 = storage->GetDictionarySync(
      GURL("https://origin.test/testfile1"), mojom::RequestDestination::kEmpty);
  ASSERT_TRUE(dict1);

  scoped_refptr<net::SharedDictionary> dict2 = storage->GetDictionarySync(
      GURL("https://origin.test/testfile2"), mojom::RequestDestination::kEmpty);
  ASSERT_TRUE(dict2);

  net::TestCompletionCallback read_callback1;
  EXPECT_EQ(net::OK, read_callback1.GetResult(
                         dict1->ReadAll(read_callback1.callback())));
  EXPECT_EQ(kTestData1,
            std::string(reinterpret_cast<const char*>(dict1->data()->data()),
                        dict1->size()));

  net::TestCompletionCallback read_callback2;
  EXPECT_EQ(net::OK, read_callback2.GetResult(
                         dict2->ReadAll(read_callback2.callback())));
  EXPECT_EQ(kTestData2,
            std::string(reinterpret_cast<const char*>(dict2->data()->data()),
                        dict2->size()));
}

TEST_F(SharedDictionaryManagerOnDiskTest, GetDictionary) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);

  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);

    // Write the test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin.test/dict"),
                    "testfile*", kTestData1);

    FlushCacheTasks();
    // Releasing `storage` and `manager`.
  }
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  // The dictionaries must be available after recreating `manager`.
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/testfile"),
                                          mojom::RequestDestination::kEmpty));
  scoped_refptr<net::SharedDictionary> dict;
  storage->GetDictionary(
      GURL("https://origin.test/testfile"), mojom::RequestDestination::kEmpty,
      base::BindLambdaForTesting(
          [&](scoped_refptr<net::SharedDictionary> dictionary) {
            dict = std::move(dictionary);
          }));
  EXPECT_FALSE(dict);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(dict);
  net::TestCompletionCallback read_callback;
  EXPECT_EQ(net::OK,
            read_callback.GetResult(dict->ReadAll(read_callback.callback())));
  EXPECT_EQ(kTestData1,
            std::string(reinterpret_cast<const char*>(dict->data()->data()),
                        dict->size()));
}

#if !BUILDFLAG(IS_FUCHSIA)
// Test that corruptted disk cache doesn't cause crash.
// CorruptDiskCache() doesn't work on Fuchsia. So disabling the following tests
// on Fuchsia.
TEST_F(SharedDictionaryManagerOnDiskTest, CorruptedDiskCacheAndWriteData) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);

  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    // Write the test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin.test/dict1"),
                    "testfile1*", kTestData1);
    FlushCacheTasks();
  }
  CorruptDiskCache();
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    FlushCacheTasks();
    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
    }
    WriteDictionary(storage.get(), GURL("https://origin.test/dict2"),
                    "testfile2*", kTestData2);
    FlushCacheTasks();

    // Writing dictionary fails, so MismatchingEntryDeletionTask cleans all
    // dictionary in the metadata store.
    EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
  }
}

TEST_F(SharedDictionaryManagerOnDiskTest, CorruptedDiskCacheAndGetData) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);

  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    // Write the test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin.test/dict1"),
                    "testfile1*", kTestData1);
    FlushCacheTasks();
  }
  CorruptDiskCache();
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    FlushCacheTasks();
    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
    }

    scoped_refptr<net::SharedDictionary> dict =
        storage->GetDictionarySync(GURL("https://origin.test/testfile1"),
                                   mojom::RequestDestination::kEmpty);
    ASSERT_TRUE(dict);

    // Reading the dictionary should fail because the disk cache is broken.
    net::TestCompletionCallback read_callback;
    EXPECT_EQ(net::ERR_FAILED,
              read_callback.GetResult(dict->ReadAll(read_callback.callback())));

    FlushCacheTasks();

    // After failing to read the disk cache entry, MismatchingEntryDeletionTask
    // cleans all dictionary in the metadata store.
    EXPECT_FALSE(
        storage->GetDictionarySync(GURL("https://origin.test/testfile1"),
                                   mojom::RequestDestination::kEmpty));
    EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
  }
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(SharedDictionaryManagerOnDiskTest, CorruptedDatabase) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);

  base::UnguessableToken token1, token2;
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    // Write the test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin.test/dict"),
                    "testfile*", kTestData1);
    FlushCacheTasks();
    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
      token1 =
          dictionary_map.begin()->second.begin()->second.disk_cache_key_token();
    }
  }
  CorruptDatabase();
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    FlushCacheTasks();
    EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
    WriteDictionary(storage.get(), GURL("https://origin.test/dict"),
                    "testfile*", kTestData1);
    FlushCacheTasks();
    // Can't add a new entry right after the databace corruption.
    EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
  }
  // Test that database corruption can be recovered after reboot.
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    FlushCacheTasks();
    EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
    WriteDictionary(storage.get(), GURL("https://origin.test/dict"),
                    "testfile*", kTestData1);
    FlushCacheTasks();
    EXPECT_FALSE(GetOnDiskDictionaryMap(storage.get()).empty());

    scoped_refptr<net::SharedDictionary> dict =
        storage->GetDictionarySync(GURL("https://origin.test/testfile"),
                                   mojom::RequestDestination::kEmpty);
    ASSERT_TRUE(dict);

    // We can read the new dictionary.
    net::TestCompletionCallback read_callback;
    EXPECT_EQ(net::OK,
              read_callback.GetResult(dict->ReadAll(read_callback.callback())));
    EXPECT_EQ(kTestData1,
              std::string(reinterpret_cast<const char*>(dict->data()->data()),
                          dict->size()));

    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
      token2 =
          dictionary_map.begin()->second.begin()->second.disk_cache_key_token();
    }

    EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));
    EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token2));

    base::RunLoop run_loop;
    manager->ClearData(
        base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
        base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
    run_loop.Run();
    FlushCacheTasks();

    EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token1));
    EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token2));
  }
}

TEST_F(SharedDictionaryManagerOnDiskTest, MetadataBrokenDatabase) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);

  base::UnguessableToken token1;
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    // Write the first test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin1.test/dict"),
                    "testfile*", kTestData1);
    FlushCacheTasks();
    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
      token1 =
          dictionary_map.begin()->second.begin()->second.disk_cache_key_token();
    }
  }
  ManipulateDatabase({"DELETE FROM meta WHERE key='total_dict_size'"});
  // Test that metadata database corruption can be recovered after reboot.
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    FlushCacheTasks();
    // Write the second test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin2.test/dict"),
                    "testfile*", kTestData2);
    FlushCacheTasks();
    // We can't write the second data.
    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
      EXPECT_EQ(GURL("https://origin1.test/dict"),
                dictionary_map.begin()->second.begin()->second.url());
    }
    // SetCacheMaxSize() triggers CacheEvictionTask which reset the storage
    // when `total_dict_size` is not available.
    manager->SetCacheMaxSize(10000);

    // RunUntilIdle() to load from the database.
    task_environment_.RunUntilIdle();
    FlushCacheTasks();
    // The data must be cleared.
    EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());

    // Now we can write the data.
    WriteDictionary(storage.get(), GURL("https://origin2.test/dict"),
                    "testfile*", kTestData2);
    FlushCacheTasks();
    // We can't write the second data.
    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
      EXPECT_EQ(GURL("https://origin2.test/dict"),
                dictionary_map.begin()->second.begin()->second.url());
    }
  }
}

TEST_F(SharedDictionaryManagerOnDiskTest, LastUsedTime) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  base::Time last_used_time_after_second_get_dict;
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    // Write the test data to the dictionary.
    WriteDictionary(storage.get(), GURL("https://origin.test/dict"),
                    "testfile*", kTestData1);
    FlushCacheTasks();

    const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
    ASSERT_EQ(1u, dictionary_map.size());
    ASSERT_EQ(1u, dictionary_map.begin()->second.size());

    base::Time initial_last_used_time =
        dictionary_map.begin()->second.begin()->second.last_used_time();

    // Move the clock forward by 1 second.
    task_environment_.FastForwardBy(base::Seconds(1));

    scoped_refptr<net::SharedDictionary> dict1 =
        storage->GetDictionarySync(GURL("https://origin.test/testfile?1"),
                                   mojom::RequestDestination::kEmpty);
    base::Time last_used_time_after_first_get_dict =
        dictionary_map.begin()->second.begin()->second.last_used_time();

    // Move the clock forward by 1 second.
    task_environment_.FastForwardBy(base::Seconds(1));

    scoped_refptr<net::SharedDictionary> dict2 =
        storage->GetDictionarySync(GURL("https://origin.test/testfile?2"),
                                   mojom::RequestDestination::kEmpty);
    last_used_time_after_second_get_dict =
        dictionary_map.begin()->second.begin()->second.last_used_time();
    EXPECT_NE(initial_last_used_time, last_used_time_after_first_get_dict);
    EXPECT_NE(last_used_time_after_first_get_dict,
              last_used_time_after_second_get_dict);
    // Releasing `dict1`, `dict2`, `storage` and `manager`.
  }
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));

  // The last_used_time data must be available after recreating `manager`.
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
  ASSERT_EQ(1u, dictionary_map.size());
  ASSERT_EQ(1u, dictionary_map.begin()->second.size());
  EXPECT_EQ(last_used_time_after_second_get_dict,
            dictionary_map.begin()->second.begin()->second.last_used_time());
}

MATCHER_P(DictionaryUrlIs,
          url,
          std::string(negation ? "doesn't have" : "has") + " " + url +
              " as the URL") {
  return arg.url().spec() == url;
}

TEST_F(SharedDictionaryManagerOnDiskTest, ClearData) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    WriteDictionary(storage.get(), GURL("https://origin.test/1"), "p1*",
                    kTestData1);
    WriteDictionary(storage.get(), GURL("https://target.test/1"), "p1*",
                    kTestData1);
    FlushCacheTasks();

    // Move the clock forward by 1 day.
    task_environment_.FastForwardBy(base::Days(1));
    WriteDictionary(storage.get(), GURL("https://origin.test/2"), "p2*",
                    kTestData1);
    WriteDictionary(storage.get(), GURL("https://target.test/2"), "p2*",
                    kTestData1);
    WriteDictionary(storage.get(), GURL("https://target.test/3"), "p3*",
                    kTestData2);
    FlushCacheTasks();

    // Move the clock forward by 1 day.
    task_environment_.FastForwardBy(base::Days(1));
    WriteDictionary(storage.get(), GURL("https://origin.test/3"), "p3*",
                    kTestData1);
    WriteDictionary(storage.get(), GURL("https://target.test/4"), "p4*",
                    kTestData1);
    FlushCacheTasks();

    // Move the clock forward by 12 hours.
    task_environment_.FastForwardBy(base::Hours(12));

    // Get a dictionary before calling ClearData().
    scoped_refptr<net::SharedDictionary> dict = storage->GetDictionarySync(
        GURL("https://target.test/p3?"), mojom::RequestDestination::kEmpty);
    ASSERT_TRUE(dict);

    base::RunLoop run_loop;
    manager->ClearData(base::Time::Now() - base::Days(2),
                       base::Time::Now() - base::Days(1),
                       base::BindRepeating([](const GURL& url) {
                         return url == GURL("https://target.test/");
                       }),
                       run_loop.QuitClosure());
    run_loop.Run();

    // The dictionaries of "https://target.test/2" and "https://target.test/3"
    // must be deleted.
    EXPECT_THAT(
        GetOnDiskDictionaryMap(storage.get()),
        ElementsAre(
            Pair(url::SchemeHostPort(GURL("https://origin.test/")),
                 ElementsAre(
                     Pair(std::make_tuple(
                              "/p1*", std::set<mojom::RequestDestination>()),
                          DictionaryUrlIs("https://origin.test/1")),
                     Pair(std::make_tuple(
                              "/p2*", std::set<mojom::RequestDestination>()),
                          DictionaryUrlIs("https://origin.test/2")),
                     Pair(std::make_tuple(
                              "/p3*", std::set<mojom::RequestDestination>()),
                          DictionaryUrlIs("https://origin.test/3")))),
            Pair(url::SchemeHostPort(GURL("https://target.test/")),
                 ElementsAre(
                     Pair(std::make_tuple(
                              "/p1*", std::set<mojom::RequestDestination>()),
                          DictionaryUrlIs("https://target.test/1")),
                     Pair(std::make_tuple(
                              "/p4*", std::set<mojom::RequestDestination>()),
                          DictionaryUrlIs("https://target.test/4"))))));

    // We can still read the deleted dictionary from `dict`.
    net::TestCompletionCallback read_callback;
    EXPECT_EQ(net::OK,
              read_callback.GetResult(dict->ReadAll(read_callback.callback())));
    EXPECT_EQ(kTestData2,
              std::string(reinterpret_cast<const char*>(dict->data()->data()),
                          dict->size()));
    // Releasing `dict`, `storage` and `manager`.
  }
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  // The dictionaries of "https://target.test/2" and "https://target.test/3"
  // must have been deleted also from the disk.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(
          Pair(url::SchemeHostPort(GURL("https://origin.test/")),
               ElementsAre(
                   Pair(std::make_tuple("/p1*",
                                        std::set<mojom::RequestDestination>()),
                        DictionaryUrlIs("https://origin.test/1")),
                   Pair(std::make_tuple("/p2*",
                                        std::set<mojom::RequestDestination>()),
                        DictionaryUrlIs("https://origin.test/2")),
                   Pair(std::make_tuple("/p3*",
                                        std::set<mojom::RequestDestination>()),
                        DictionaryUrlIs("https://origin.test/3")))),
          Pair(url::SchemeHostPort(GURL("https://target.test/")),
               ElementsAre(
                   Pair(std::make_tuple("/p1*",
                                        std::set<mojom::RequestDestination>()),
                        DictionaryUrlIs("https://target.test/1")),
                   Pair(std::make_tuple("/p4*",
                                        std::set<mojom::RequestDestination>()),
                        DictionaryUrlIs("https://target.test/4"))))));
}

TEST_F(SharedDictionaryManagerOnDiskTest, ClearDataSerializedOperation) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  WriteDictionary(storage.get(), GURL("https://target1.test/d"), "p*",
                  kTestData1);
  WriteDictionary(storage.get(), GURL("https://target2.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();

  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(
          Pair(
              url::SchemeHostPort(GURL("https://target1.test/")),
              ElementsAre(Pair(
                  std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                  DictionaryUrlIs("https://target1.test/d")))),
          Pair(
              url::SchemeHostPort(GURL("https://target2.test/")),
              ElementsAre(Pair(
                  std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                  DictionaryUrlIs("https://target2.test/d"))))));

  base::RunLoop run_loop1;
  manager->ClearData(base::Time(), base::Time::Max(),
                     base::BindRepeating([](const GURL& url) {
                       return url == GURL("https://target1.test/");
                     }),
                     run_loop1.QuitClosure());
  base::RunLoop run_loop2;
  manager->ClearData(base::Time(), base::Time::Max(),
                     base::BindRepeating([](const GURL& url) {
                       return url == GURL("https://target2.test/");
                     }),
                     run_loop2.QuitClosure());
  run_loop1.Run();

  // The dictionary of "https://target2.test/" must still alive, because the
  // operation of ClearData is serialized.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target2.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target2.test/d"))))));

  run_loop2.Run();

  EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
}

TEST_F(SharedDictionaryManagerOnDiskTest, ClearDataForIsolationKey) {
  net::SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl),
                                                   kSite);
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://different-origin.test")), kSite);
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage1 =
        manager->GetStorage(isolation_key1);
    ASSERT_TRUE(storage1);
    WriteDictionary(storage1.get(), GURL("https://origin1.test/d"), "p*",
                    kTestData1);
    WriteDictionary(storage1.get(), GURL("https://origin2.test/d"), "p*",
                    kTestData2);

    scoped_refptr<SharedDictionaryStorage> storage2 =
        manager->GetStorage(isolation_key2);
    ASSERT_TRUE(storage2);
    WriteDictionary(storage2.get(), GURL("https://origin1.test/d"), "p*",
                    kTestData1);
    FlushCacheTasks();

    // Get a dictionary before calling ClearDataForIsolationKey().
    scoped_refptr<net::SharedDictionary> dict = storage1->GetDictionarySync(
        GURL("https://origin1.test/p?"), mojom::RequestDestination::kEmpty);
    ASSERT_TRUE(dict);

    base::RunLoop run_loop;
    manager->ClearDataForIsolationKey(isolation_key1, run_loop.QuitClosure());
    run_loop.Run();

    // The dictionaries for `isolation_key1` must have been deleted.
    EXPECT_TRUE(GetOnDiskDictionaryMap(storage1.get()).empty());
    EXPECT_THAT(
        GetOnDiskDictionaryMap(storage2.get()),
        ElementsAre(Pair(
            url::SchemeHostPort(GURL("https://origin1.test/")),
            ElementsAre(Pair(
                std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                DictionaryUrlIs("https://origin1.test/d"))))));

    // We can still read the deleted dictionary from `dict`.
    net::TestCompletionCallback read_callback;
    EXPECT_EQ(net::OK,
              read_callback.GetResult(dict->ReadAll(read_callback.callback())));
    EXPECT_EQ(kTestData1,
              std::string(reinterpret_cast<const char*>(dict->data()->data()),
                          dict->size()));
    // Releasing `dict`, `storage` and `manager`.
  }
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  ASSERT_TRUE(storage1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  ASSERT_TRUE(storage2);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(GetOnDiskDictionaryMap(storage1.get()).empty());
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage2.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://origin1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://origin1.test/d"))))));
}

TEST_F(SharedDictionaryManagerOnDiskTest, ExpiredDictionaryDeletionOnReload) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  base::UnguessableToken token1, token2;
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    WriteDictionaryWithExpiry(storage.get(), GURL("https://target1.test/d"),
                              "p*", base::Seconds(10), kTestData1);
    WriteDictionaryWithExpiry(storage.get(), GURL("https://target2.test/d"),
                              "p*", base::Seconds(11), kTestData2);
    // FlushCacheTasks() to finish the persistence operation.
    FlushCacheTasks();

    const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
    token1 = GetDiskCacheKeyTokenOfFirstDictionary(dictionary_map,
                                                   "https://target1.test/");
    token2 = GetDiskCacheKeyTokenOfFirstDictionary(dictionary_map,
                                                   "https://target2.test/");
    // Releasing `storage` and `manager`.
  }

  // Move the clock forward by 10 second.
  task_environment_.FastForwardBy(base::Seconds(10));

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  // The first dictionary must have been deleted.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target2.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target2.test/d"))))));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token1));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token2));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       ExpiredDictionaryDeletionOnNewDictionary) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  // Write a first dictionary.
  WriteDictionaryWithExpiry(storage.get(), GURL("https://target1.test/d"), "p*",
                            base::Seconds(10), kTestData1);
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();
  base::UnguessableToken token1 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target1.test/");

  // Move the clock forward by 10 second.
  task_environment_.FastForwardBy(base::Seconds(10));
  // The first dictionary still exists.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));

  // Write a second dictionary.
  WriteDictionaryWithExpiry(storage.get(), GURL("https://target2.test/d"), "p*",
                            base::Seconds(10), kTestData2);
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();
  base::UnguessableToken token2 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target2.test/");

  // The first dictionary must have been deleted.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target2.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target2.test/d"))))));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token1));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token2));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       ExpiredDictionaryDeletionOnSetCacheMaxSize) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  WriteDictionaryWithExpiry(storage.get(), GURL("https://target1.test/d"), "p*",
                            base::Seconds(10), kTestData1);
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();
  base::UnguessableToken token = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target1.test/");

  // Move the clock forward by 10 second.
  task_environment_.FastForwardBy(base::Seconds(10));
  // The first dictionary still exists.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));

  // Set the max size to kTestData1.size() * 100
  manager->SetCacheMaxSize(kTestData1.size() * 100);

  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  // The dictionary must be deleted.
  EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       ExpiredDictionaryDeletionOnClearData) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  WriteDictionaryWithExpiry(storage.get(), GURL("https://target1.test/d"), "p*",
                            base::Seconds(10), kTestData1);
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();
  base::UnguessableToken token = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target1.test/");

  // Move the clock forward by 10 second.
  task_environment_.FastForwardBy(base::Seconds(10));
  // The first dictionary still exists.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));

  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  // The dictionary must be deleted.
  EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       ExpiredDictionaryDeletionOnClearDataForIsolationKey) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  WriteDictionaryWithExpiry(storage.get(), GURL("https://target1.test/d"), "p*",
                            base::Seconds(10), kTestData1);
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();
  base::UnguessableToken token = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target1.test/");

  // Move the clock forward by 10 second.
  task_environment_.FastForwardBy(base::Seconds(10));
  // The first dictionary still exists.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));

  base::RunLoop run_loop;
  manager->ClearDataForIsolationKey(
      net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL("https://different-origin.test")), kSite),
      run_loop.QuitClosure());
  run_loop.Run();

  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  // The dictionary must be deleted.
  EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token));
}

TEST_F(SharedDictionaryManagerOnDiskTest, CacheEvictionOnReload) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  base::UnguessableToken token1, token2, token3;
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    WriteDictionary(storage.get(), GURL("https://target1.test/d"), "p*",
                    kTestData1);
    task_environment_.FastForwardBy(base::Seconds(1));
    WriteDictionary(storage.get(), GURL("https://target2.test/d"), "p*",
                    kTestData1);
    task_environment_.FastForwardBy(base::Seconds(1));
    WriteDictionary(storage.get(), GURL("https://target3.test/d"), "p*",
                    kTestData1);
    // FlushCacheTasks() to finish the persistence operation.
    FlushCacheTasks();

    const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
    token1 = GetDiskCacheKeyTokenOfFirstDictionary(dictionary_map,
                                                   "https://target1.test/");
    token2 = GetDiskCacheKeyTokenOfFirstDictionary(dictionary_map,
                                                   "https://target2.test/");
    token3 = GetDiskCacheKeyTokenOfFirstDictionary(dictionary_map,
                                                   "https://target3.test/");

    // Releasing `storage` and `manager`.
  }

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager(/*cache_max_size=*/kTestData1.size() * 2);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  // Only the third dictionary exists.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target3.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target3.test/d"))))));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token1));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token2));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token3));
}

TEST_F(SharedDictionaryManagerOnDiskTest, CacheEvictionOnSetCacheMaxSize) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  WriteDictionary(storage.get(), GURL("https://target1.test/d"), "p*",
                  kTestData1);
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage.get(), GURL("https://target2.test/d"), "p*",
                  kTestData1);
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage.get(), GURL("https://target3.test/d"), "p*",
                  kTestData1);
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
  base::UnguessableToken token1 = GetDiskCacheKeyTokenOfFirstDictionary(
      dictionary_map, "https://target1.test/");
  base::UnguessableToken token2 = GetDiskCacheKeyTokenOfFirstDictionary(
      dictionary_map, "https://target2.test/");
  base::UnguessableToken token3 = GetDiskCacheKeyTokenOfFirstDictionary(
      dictionary_map, "https://target3.test/");

  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 2);
  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  // Only the third dictionary exists.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target3.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target3.test/d"))))));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token1));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token2));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token3));
}

TEST_F(SharedDictionaryManagerOnDiskTest, CacheEvictionOnNewDictionary) {
  const net::SchemefulSite site1(GURL("https://site1.test"));
  const net::SchemefulSite site2(GURL("https://site2.test"));
  const net::SchemefulSite site3(GURL("https://site3.test"));

  net::SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://origin1.test")), site1);
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://origin2.test")), site2);
  net::SharedDictionaryIsolationKey isolation_key3(
      url::Origin::Create(GURL("https://origin3.test")), site3);

  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 2);

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  scoped_refptr<SharedDictionaryStorage> storage3 =
      manager->GetStorage(isolation_key3);

  WriteDictionary(storage1.get(), GURL("https://target1.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token1 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage1.get()), "https://target1.test/");

  task_environment_.FastForwardBy(base::Seconds(1));

  WriteDictionary(storage2.get(), GURL("https://target2.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token2 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage2.get()), "https://target2.test/");

  task_environment_.FastForwardBy(base::Seconds(1));

  // Both the dictinaries exist.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage1.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage2.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target2.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target2.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token2));

  WriteDictionary(storage3.get(), GURL("https://target3.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token3 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage3.get()), "https://target3.test/");

  // Only the third dictionary exists.
  EXPECT_TRUE(GetOnDiskDictionaryMap(storage1.get()).empty());
  EXPECT_TRUE(GetOnDiskDictionaryMap(storage2.get()).empty());
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage3.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target3.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target3.test/d"))))));

  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token1));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token2));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token3));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       CacheEvictionPerSiteExceededSizeLimit) {
  const net::SchemefulSite site1(GURL("https://site1.test"));
  const net::SchemefulSite site2(GURL("https://site2.test"));

  net::SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://origin1.test")), site1);
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://origin1.test")), site2);
  // The top frame site of `isolation_key3` is same as the top frame site of
  // `isolation_key2`.
  net::SharedDictionaryIsolationKey isolation_key3(
      url::Origin::Create(GURL("https://origin2.test")), site2);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 2);

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  scoped_refptr<SharedDictionaryStorage> storage3 =
      manager->GetStorage(isolation_key3);

  // Register the first dictionary.
  WriteDictionary(storage1.get(), GURL("https://target1.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token1 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage1.get()), "https://target1.test/");
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage1.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));
  task_environment_.FastForwardBy(base::Seconds(1));

  // Register the second dictionary.
  WriteDictionary(storage2.get(), GURL("https://target2.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token2 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage2.get()), "https://target2.test/");
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage2.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target2.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target2.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token2));
  task_environment_.FastForwardBy(base::Seconds(1));

  // Register the third dictionary.
  WriteDictionary(storage3.get(), GURL("https://target3.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token3 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage3.get()), "https://target3.test/");
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage3.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target3.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target3.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token3));

  // The first dictionary must still exist.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage1.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));

  // The second dictionary must have been deleted because the size limit per
  // site is kTestData1.size() * 2 / 2.
  EXPECT_TRUE(GetOnDiskDictionaryMap(storage2.get()).empty());
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token2));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       CacheEvictionPerSiteExceededCountLimit) {
  const net::SchemefulSite site1(GURL("https://site1.test"));
  const net::SchemefulSite site2(GURL("https://site2.test"));

  net::SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://origin1.test")), site1);
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://origin1.test")), site2);
  // The top frame site of `isolation_key3` is same as the top frame site of
  // `isolation_key2`.
  net::SharedDictionaryIsolationKey isolation_key3(
      url::Origin::Create(GURL("https://origin2.test")), site2);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager(/*cache_max_size=*/0,
                                    /*cache_max_count=*/4);

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  scoped_refptr<SharedDictionaryStorage> storage3 =
      manager->GetStorage(isolation_key3);

  // Register the first dictionary.
  WriteDictionary(storage1.get(), GURL("https://target1.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token1 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage1.get()), "https://target1.test/");
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage1.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));
  task_environment_.FastForwardBy(base::Seconds(1));

  // Register the second dictionary.
  WriteDictionary(storage2.get(), GURL("https://target2.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token2 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage2.get()), "https://target2.test/");
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage2.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target2.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target2.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token2));
  task_environment_.FastForwardBy(base::Seconds(1));

  // Register the third dictionary.
  WriteDictionary(storage2.get(), GURL("https://target3.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token3 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage2.get()), "https://target3.test/");
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage2.get()),
      ElementsAre(
          Pair(
              url::SchemeHostPort(GURL("https://target2.test/")),
              ElementsAre(Pair(
                  std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                  DictionaryUrlIs("https://target2.test/d")))),
          Pair(
              url::SchemeHostPort(GURL("https://target3.test/")),
              ElementsAre(Pair(
                  std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                  DictionaryUrlIs("https://target3.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token3));

  // Register the fourth dictionary.
  WriteDictionary(storage3.get(), GURL("https://target4.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token4 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage3.get()), "https://target4.test/");
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage3.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target4.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target4.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token4));

  // The first dictionary must still exist.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage1.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));

  // The third dictionary must still exist. But the second dictionary must have
  // been deleted because the count limit per site is 2 (4 / 2).
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage2.get()),
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target3.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target3.test/d"))))));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token2));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token3));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       CacheEvictionAfterUpdatingLastUsedTime) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  // Write dictionary 1.
  WriteDictionary(storage.get(), GURL("https://target1.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token1 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target1.test/");

  task_environment_.FastForwardBy(base::Seconds(1));

  // Write dictionary 2.
  WriteDictionary(storage.get(), GURL("https://target2.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token2 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target2.test/");

  task_environment_.FastForwardBy(base::Seconds(1));

  // Write dictionary 3.
  WriteDictionary(storage.get(), GURL("https://target3.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token3 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target3.test/");

  task_environment_.FastForwardBy(base::Seconds(1));

  // Write dictionary 4.
  WriteDictionary(storage.get(), GURL("https://target4.test/d"), "p*",
                  kTestData1);
  FlushCacheTasks();
  base::UnguessableToken token4 = GetDiskCacheKeyTokenOfFirstDictionary(
      GetOnDiskDictionaryMap(storage.get()), "https://target4.test/");

  task_environment_.FastForwardBy(base::Seconds(1));

  // Call GetDictionary to update the last used time of the dictionary 1.
  scoped_refptr<net::SharedDictionary> dict1 = storage->GetDictionarySync(
      GURL("https://target1.test/path?"), mojom::RequestDestination::kEmpty);
  ASSERT_TRUE(dict1);

  // Set the max size to kTestData1.size() * 3. The low water mark will be
  // kTestData1.size() * 2.7 (3 * 0.9).
  manager->SetCacheMaxSize(kTestData1.size() * 3);

  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();
  // The dictionary 1 and 4 must still exist.
  EXPECT_THAT(
      GetOnDiskDictionaryMap(storage.get()),
      ElementsAre(
          Pair(
              url::SchemeHostPort(GURL("https://target1.test/")),
              ElementsAre(Pair(
                  std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                  DictionaryUrlIs("https://target1.test/d")))),
          Pair(
              url::SchemeHostPort(GURL("https://target4.test/")),
              ElementsAre(Pair(
                  std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                  DictionaryUrlIs("https://target4.test/d"))))));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token1));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token2));
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), token3));
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), token4));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       MismatchingEntryDeletionMetadataUnavailableDictionary) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  const std::string entry_key = token.ToString();
  const std::string kTestData = "Hello world";
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  WriteDiskCacheEntry(manager.get(), entry_key, kTestData);
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), entry_key));

  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), entry_key));
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), entry_key));
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.DiskCacheEntryMissingDictionaryCount",
      0, 1);
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       MismatchingEntryDeletionInvalidDiskCacheEntry) {
  const std::string kTestKey = "test";
  const std::string kTestData = "Hello world";
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  WriteDiskCacheEntry(manager.get(), kTestKey, kTestData);
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), kTestKey));

  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), kTestKey));
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DiskCacheEntryExists(manager.get(), kTestKey));
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.DiskCacheEntryMissingDictionaryCount",
      0, 1);
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       MismatchingEntryDeletionDiskCacheEntryUnavailableDictionary) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  {
    std::unique_ptr<SharedDictionaryManager> manager =
        CreateSharedDictionaryManager();
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    ASSERT_TRUE(storage);
    WriteDictionary(storage.get(), GURL("https://target1.test/d"), "p*",
                    kTestData1);
    // FlushCacheTasks() to finish the persistence operation.
    FlushCacheTasks();

    const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
    EXPECT_THAT(
        dictionary_map,
        ElementsAre(Pair(
            url::SchemeHostPort(GURL("https://target1.test/")),
            ElementsAre(Pair(
                std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
                DictionaryUrlIs("https://target1.test/d"))))));

    DoomDiskCacheEntry(manager.get(),
                       GetDiskCacheKeyTokenOfFirstDictionary(
                           dictionary_map, "https://target1.test/"));

    base::RunLoop run_loop;
    manager->ClearData(
        base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
        base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_FALSE(GetOnDiskDictionaryMap(storage.get()).empty());

    base::HistogramTester histogram_tester;
    task_environment_.RunUntilIdle();

    EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());

    histogram_tester.ExpectUniqueSample(
        "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount", 0,
        1);
    histogram_tester.ExpectUniqueSample(
        "Net.SharedDictionaryManagerOnDisk."
        "DiskCacheEntryMissingDictionaryCount",
        1, 1);

    // Releasing `storage` and `manager`.
  }
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // RunUntilIdle() to load from the database.
  task_environment_.RunUntilIdle();

  // The storage must be empty.
  EXPECT_TRUE(GetOnDiskDictionaryMap(storage.get()).empty());
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       MismatchingEntryDeletionCanBeTriggeredOnlyOnce) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  // Calls ClearData() to trigger MismatchingEntryDeletionTask.
  base::RunLoop run_loop1;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop1.QuitClosure());
  run_loop1.Run();

  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.DiskCacheEntryMissingDictionaryCount",
      0, 1);

  const base::UnguessableToken token = base::UnguessableToken::Create();
  const std::string entry_key = token.ToString();
  const std::string kTestData = "Hello world";

  WriteDiskCacheEntry(manager.get(), entry_key, kTestData);
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), entry_key));

  base::RunLoop run_loop2;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), entry_key));
  task_environment_.RunUntilIdle();
  // MismatchingEntryDeletionTask can be triggered only once. So the disk cache
  // entry should not be deleted.
  EXPECT_TRUE(DiskCacheEntryExists(manager.get(), entry_key));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       MismatchingEntryDeletionWritingEntryMustNotBeDeleted) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  // Start writing a dictionary.
  const std::string use_as_dictionary_header = "match=\"/p*\"";
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": ", use_as_dictionary_header, "\n", kDefaultCacheControlHeader,
           "\n"}));
  ASSERT_TRUE(headers);
  base::expected<scoped_refptr<SharedDictionaryWriter>,
                 mojom::SharedDictionaryError>
      writer = SharedDictionaryStorage::MaybeCreateWriter(
          use_as_dictionary_header, /*shared_dictionary_writer_enabled=*/true,
          storage.get(), mojom::RequestMode::kSameOrigin,
          mojom::FetchResponseType::kBasic, GURL("https://target1.test/d"),
          /*request_time=*/base::Time::Now(),
          /*response_time=*/base::Time::Now(), *headers,
          /*was_fetched_via_cache=*/false, DummyAccessAllowedCheckCallback());
  ASSERT_TRUE(writer.has_value());
  ASSERT_TRUE(*writer);
  (*writer)->Append(kTestData1.c_str(), kTestData1.size());

  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  base::HistogramTester histogram_tester;
  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.DiskCacheEntryMissingDictionaryCount",
      0, 1);

  // Finish writing the dictionary.
  (*writer)->Finish();

  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  // There should be no change in MismatchingEntryDeletionTask related metrics.
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.DiskCacheEntryMissingDictionaryCount",
      0, 1);

  const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
  EXPECT_THAT(
      dictionary_map,
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       MismatchingEntryDeletionWritingDiskCacheEntryMustNotBeDeleted) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl),
                                                  kSite);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  // Write a dictionary.
  WriteDictionary(storage.get(), GURL("https://target1.test/d"), "p*",
                  kTestData1);
  // But do not call FlushCacheTasks() to keep writing the disk cache entry.
  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  base::HistogramTester histogram_tester;

  // FlushCacheTasks() to finish the persistence operation.
  FlushCacheTasks();

  const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
  EXPECT_THAT(
      dictionary_map,
      ElementsAre(Pair(
          url::SchemeHostPort(GURL("https://target1.test/")),
          ElementsAre(Pair(
              std::make_tuple("/p*", std::set<mojom::RequestDestination>()),
              DictionaryUrlIs("https://target1.test/d"))))));
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.InvalidDiskCacheEntryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.MetadataMissingDictionaryCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionaryManagerOnDisk.DiskCacheEntryMissingDictionaryCount",
      0, 1);
}

}  // namespace network
