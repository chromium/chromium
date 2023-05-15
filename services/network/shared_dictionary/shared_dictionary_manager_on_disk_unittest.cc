// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
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
#include "services/network/shared_dictionary/shared_dictionary.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

const GURL kUrl("https://origin.test/");
const net::SchemefulSite kSite(kUrl);
const std::string kTestData1 = "Hello world";
const std::string kTestData2 = "Bonjour le monde";

void WriteDictionary(SharedDictionaryStorage* storage,
                     const GURL& dictionary_url,
                     const std::string& match,
                     const std::string& data) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"/", match, "\"\n\n"}));
  ASSERT_TRUE(headers);
  scoped_refptr<SharedDictionaryWriter> writer =
      storage->MaybeCreateWriter(dictionary_url, base::Time::Now(), *headers);
  ASSERT_TRUE(writer);
  writer->Append(data.c_str(), data.size());
  writer->Finish();
}

bool DiskCacheEntryExists(SharedDictionaryManager* manager,
                          const base::UnguessableToken& disk_cache_key_token) {
  TestEntryResultCompletionCallback open_callback;
  disk_cache::EntryResult open_result = open_callback.GetResult(
      static_cast<SharedDictionaryManagerOnDisk*>(manager)
          ->disk_cache()
          .OpenOrCreateEntry(disk_cache_key_token.ToString(),
                             /*create=*/false, open_callback.callback()));
  return open_result.net_error() == net::OK;
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
  std::unique_ptr<SharedDictionaryManager> CreateSharedDictionaryManager() {
    return SharedDictionaryManager::CreateOnDisk(
        database_path_, cache_directory_path_,
#if BUILDFLAG(IS_ANDROID)
        /*app_status_listener=*/nullptr,
#endif  // BUILDFLAG(IS_ANDROID)
        /*file_operations_factory=*/nullptr);
  }
  const std::map<url::SchemeHostPort,
                 std::map<std::string, net::SharedDictionaryInfo>>&
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

 private:
  base::test::TaskEnvironment task_environment_;
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
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  WriteDictionary(storage.get(), GURL("https://origin.test/dict"), "testfile*",
                  kTestData1);

  FlushCacheTasks();

  // Check the returned dictionary from GetDictionary().
  std::unique_ptr<SharedDictionary> dict1 =
      storage->GetDictionary(GURL("https://origin.test/testfile?1"));
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
  std::unique_ptr<SharedDictionary> dict2 =
      storage->GetDictionary(GURL("https://origin.test/testfile?2"));
  ASSERT_TRUE(dict2);
  // `dict2` shares the same RefCountedSharedDictionary with `dict1`. So
  // ReadAll() must synchronously return OK.
  EXPECT_EQ(net::OK, dict2->ReadAll(base::BindLambdaForTesting(
                         [&](int rv) { NOTREACHED(); })));
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
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  manager.reset();

  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"/testfile*\"\n\n"}));
  ASSERT_TRUE(headers);

  // MaybeCreateWriter() must return nullptr, after `manager` was deleted.
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      GURL("https://origin.test/dict"), base::Time::Now(), *headers);
  EXPECT_FALSE(writer);
}

TEST_F(SharedDictionaryManagerOnDiskTest, GetDictionaryAfterManagerDeleted) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  manager.reset();

  // GetDictionary() must return nullptr, after `manager` was deleted.
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionary(GURL("https://origin.test/testfile?1"));
  EXPECT_FALSE(dict);
}

TEST_F(SharedDictionaryManagerOnDiskTest,
       DictionaryWrittenInDiskCacheAfterManagerDeleted) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);
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
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);
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

  // Check the returned dictionary from GetDictionary().
  std::unique_ptr<SharedDictionary> dict1 =
      storage->GetDictionary(GURL("https://origin.test/testfile"));
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

  std::unique_ptr<SharedDictionary> dict2 =
      storage->GetDictionary(GURL("https://origin.test/testfile"));
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
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);

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

    std::unique_ptr<SharedDictionary> dict1 =
        storage->GetDictionary(GURL("https://origin.test/testfile1"));
    ASSERT_TRUE(dict1);

    std::unique_ptr<SharedDictionary> dict2 =
        storage->GetDictionary(GURL("https://origin.test/testfile2"));
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

  // The dictionaries must be available after recreating `manager`.
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  FlushCacheTasks();

  const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
  ASSERT_EQ(1u, dictionary_map.size());
  ASSERT_EQ(2u, dictionary_map.begin()->second.size());

  std::unique_ptr<SharedDictionary> dict1 =
      storage->GetDictionary(GURL("https://origin.test/testfile1"));
  ASSERT_TRUE(dict1);

  std::unique_ptr<SharedDictionary> dict2 =
      storage->GetDictionary(GURL("https://origin.test/testfile2"));
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

#if !BUILDFLAG(IS_FUCHSIA)
// Test that corruptted disk cache doesn't cause crash.
// CorruptDiskCache() doesn't work on Fuchsia. So disabling the following tests
// on Fuchsia.
TEST_F(SharedDictionaryManagerOnDiskTest, CorruptedDiskCache) {
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);

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
    // Currently, if the disk cache is corrupted, it just prevents adding new
    // dictionaries.
    // TODO(crbug.com/1413922): Implement a garbage collection logic to remove
    // the entry in the database when its disk cache entry is unavailable.
    {
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      ASSERT_EQ(1u, dictionary_map.size());
      ASSERT_EQ(1u, dictionary_map.begin()->second.size());
    }
  }
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(SharedDictionaryManagerOnDiskTest, CorruptedDatabase) {
  net::SharedDictionaryStorageIsolationKey isolation_key(
      url::Origin::Create(kUrl), kSite);

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

    std::unique_ptr<SharedDictionary> dict =
        storage->GetDictionary(GURL("https://origin.test/testfile"));
    ASSERT_TRUE(dict);

    // We can read the new dictionary.
    net::TestCompletionCallback read_callback;
    EXPECT_EQ(net::OK,
              read_callback.GetResult(dict->ReadAll(read_callback.callback())));
    EXPECT_EQ(kTestData1,
              std::string(reinterpret_cast<const char*>(dict->data()->data()),
                          dict->size()));
    // Currently the disk cache entries that were added before the database
    // corruption will not be removed.
    // TODO(crbug.com/1413922): Implement a garbage collection logic to remove
    // the entry in the disk cache when its database entry is unavailable.
  }
}

}  // namespace network
