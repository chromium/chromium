// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/shared_dictionary/shared_dictionary.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::UnorderedElementsAreArray;

namespace network {

namespace {

enum class TestManagerType {
  kInMemory,
  kOnDisk,
};

const GURL kUrl1("https://origin1.test/");
const GURL kUrl2("https://origin2.test/");
const GURL kUrl3("https://origin3.test/");
const net::SchemefulSite kSite1(kUrl1);
const net::SchemefulSite kSite2(kUrl2);
const net::SchemefulSite kSite3(kUrl3);

const std::string kTestData1 = "Hello world";
const std::string kTestData2 = "Bonjour le monde";

// The SHA256 hash of kTestData1
const net::SHA256HashValue kTestData1Hash = {
    {0x64, 0xec, 0x88, 0xca, 0x00, 0xb2, 0x68, 0xe5, 0xba, 0x1a, 0x35,
     0x67, 0x8a, 0x1b, 0x53, 0x16, 0xd2, 0x12, 0xf4, 0xf3, 0x66, 0xb2,
     0x47, 0x72, 0x32, 0x53, 0x4a, 0x8a, 0xec, 0xa3, 0x7f, 0x3c}};
// The SHA256 hash of kTestData2
const net::SHA256HashValue kTestData2Hash = {
    {0x4d, 0xc4, 0x5b, 0x5e, 0xd3, 0xde, 0x20, 0x2a, 0x56, 0x93, 0xc9,
     0x26, 0xca, 0xf9, 0x5b, 0xd9, 0x71, 0x0b, 0xef, 0x4f, 0xe5, 0xfb,
     0x16, 0xa1, 0xc2, 0x4a, 0x08, 0xed, 0x42, 0x8e, 0x8a, 0xe0}};

const size_t kCacheMaxCount = 100;

void CheckDiskCacheEntryDataEquals(
    SharedDictionaryDiskCache& disk_cache,
    const base::UnguessableToken& disk_cache_key_token,
    const std::string& expected_data) {
  TestEntryResultCompletionCallback open_callback;
  disk_cache::EntryResult open_result = open_callback.GetResult(
      disk_cache.OpenOrCreateEntry(disk_cache_key_token.ToString(),
                                   /*create=*/false, open_callback.callback()));
  EXPECT_EQ(net::OK, open_result.net_error());
  disk_cache::ScopedEntryPtr entry;
  entry.reset(open_result.ReleaseEntry());
  ASSERT_TRUE(entry);

  EXPECT_EQ(base::checked_cast<int32_t>(expected_data.size()),
            entry->GetDataSize(/*index=*/1));

  scoped_refptr<net::IOBufferWithSize> read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(expected_data.size());
  net::TestCompletionCallback read_callback;
  EXPECT_EQ(read_buffer->size(),
            read_callback.GetResult(entry->ReadData(
                /*index=*/1, /*offset=*/0, read_buffer.get(),
                expected_data.size(), read_callback.callback())));
  EXPECT_EQ(expected_data,
            std::string(reinterpret_cast<const char*>(read_buffer->data()),
                        read_buffer->size()));
}

void WriteDictionary(SharedDictionaryStorage* storage,
                     const GURL& dictionary_url,
                     const std::string& match,
                     const std::vector<std::string>& data_list,
                     const std::string& additional_options = std::string()) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"/", match, "\"", additional_options, "\n\n"}));
  ASSERT_TRUE(headers);
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      dictionary_url, base::Time::Now(), *headers,
      /*was_fetched_via_cache=*/false,
      /*access_allowed_check_callback=*/base::BindOnce([]() { return true; }));
  ASSERT_TRUE(writer);
  for (const std::string& data : data_list) {
    writer->Append(data.c_str(), data.size());
  }
  writer->Finish();
}

base::TimeDelta GetDefaultExpiration() {
  return base::FeatureList::IsEnabled(
             network::features::kCompressionDictionaryTransport)
             ? shared_dictionary::kDefaultExpiration
             : shared_dictionary::kMaxExpirationForOriginTrial;
}

}  // namespace

class SharedDictionaryManagerTest
    : public ::testing::Test,
      public testing::WithParamInterface<TestManagerType> {
 public:
  SharedDictionaryManagerTest() = default;
  ~SharedDictionaryManagerTest() override = default;

  SharedDictionaryManagerTest(const SharedDictionaryManagerTest&) = delete;
  SharedDictionaryManagerTest& operator=(const SharedDictionaryManagerTest&) =
      delete;

  void SetUp() override {
    if (GetParam() == TestManagerType::kOnDisk) {
      ASSERT_TRUE(tmp_directory_.CreateUniqueTempDir());
      database_path_ = tmp_directory_.GetPath().Append(FILE_PATH_LITERAL("db"));
      cache_directory_path_ =
          tmp_directory_.GetPath().Append(FILE_PATH_LITERAL("cache"));
    }
  }
  void TearDown() override {
    if (GetParam() == TestManagerType::kOnDisk) {
      FlushCacheTasks();
    }
  }

 protected:
  std::unique_ptr<SharedDictionaryManager> CreateSharedDictionaryManager() {
    switch (GetParam()) {
      case TestManagerType::kInMemory:
        return SharedDictionaryManager::CreateInMemory(/*cache_max_size=*/0,
                                                       kCacheMaxCount);
      case TestManagerType::kOnDisk:
        return SharedDictionaryManager::CreateOnDisk(
            database_path_, cache_directory_path_, /*cache_max_size=*/0,
            kCacheMaxCount,
#if BUILDFLAG(IS_ANDROID)
            disk_cache::ApplicationStatusListenerGetter(),
#endif  // BUILDFLAG(IS_ANDROID)
            /*file_operations_factory=*/nullptr);
    }
  }
  const std::map<
      url::SchemeHostPort,
      std::map<std::string, SharedDictionaryStorageInMemory::DictionaryInfo>>&
  GetInMemoryDictionaryMap(SharedDictionaryStorage* storage) {
    return static_cast<SharedDictionaryStorageInMemory*>(storage)
        ->GetDictionaryMap();
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

  std::vector<network::mojom::SharedDictionaryInfoPtr> GetSharedDictionaryInfo(
      SharedDictionaryManager* manager,
      const net::SharedDictionaryIsolationKey& isolation_key) {
    base::test::TestFuture<std::vector<network::mojom::SharedDictionaryInfoPtr>>
        result;
    manager->GetSharedDictionaryInfo(isolation_key, result.GetCallback());
    return result.Take();
  }

  std::vector<url::Origin> GetOriginsBetween(SharedDictionaryManager* manager,
                                             base::Time start_time,
                                             base::Time end_time) {
    base::test::TestFuture<const std::vector<url::Origin>&> result;
    manager->GetOriginsBetween(start_time, end_time, result.GetCallback());
    return result.Get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::ScopedTempDir tmp_directory_;
  base::FilePath database_path_;
  base::FilePath cache_directory_path_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedDictionaryManagerTest,
    testing::ValuesIn({TestManagerType::kInMemory, TestManagerType::kOnDisk}),
    [](const testing::TestParamInfo<TestManagerType>& info) {
      switch (info.param) {
        case TestManagerType::kInMemory:
          return "InMemory";
        case TestManagerType::kOnDisk:
          return "OnDisk";
      }
    });

TEST_P(SharedDictionaryManagerTest, SameStorageForSameIsolationKey) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  net::SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                   kSite1);
  net::SharedDictionaryIsolationKey isolation_key2(url::Origin::Create(kUrl1),
                                                   kSite1);

  EXPECT_EQ(isolation_key1, isolation_key1);

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);

  EXPECT_TRUE(storage1);
  EXPECT_TRUE(storage2);
  EXPECT_EQ(storage1.get(), storage2.get());
}

TEST_P(SharedDictionaryManagerTest, DifferentStorageForDifferentIsolationKey) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  net::SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                   kSite1);
  net::SharedDictionaryIsolationKey isolation_key2(url::Origin::Create(kUrl2),
                                                   kSite2);
  EXPECT_NE(isolation_key1, isolation_key2);

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);

  EXPECT_TRUE(storage1);
  EXPECT_TRUE(storage2);
  EXPECT_NE(storage1.get(), storage2.get());
}

TEST_P(SharedDictionaryManagerTest, CachedStorage) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "p*",
                  {"Hello"});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));

  storage.reset();

  // Even after resetting `storage`, `storage` should be in `manager`'s
  // `cached_storages_`. So the metadata is still in the memory.
  storage = manager->GetStorage(isolation_key);
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));
}

TEST_P(SharedDictionaryManagerTest, CachedStorageEvicted) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "p*",
                  {"Hello"});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));

  storage.reset();

  for (int i = 0; i < 9; ++i) {
    const GURL url = GURL(base::StringPrintf("https://test%d.test", i));
    scoped_refptr<SharedDictionaryStorage> tmp_storage =
        manager->GetStorage(net::SharedDictionaryIsolationKey(
            url::Origin::Create(url), net::SchemefulSite(url)));
    EXPECT_TRUE(tmp_storage);
  }

  // Even after creating 10 (kCachedStorageMaxSize) storages, the first storage
  // should still be in the cache.
  storage = manager->GetStorage(isolation_key);
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));
  storage.reset();

  for (int i = 0; i < 10; ++i) {
    const GURL url = GURL(base::StringPrintf("https://test%d.test", i));
    scoped_refptr<SharedDictionaryStorage> tmp_storage =
        manager->GetStorage(net::SharedDictionaryIsolationKey(
            url::Origin::Create(url), net::SchemefulSite(url)));
    EXPECT_TRUE(tmp_storage);
  }

  // When we create 11 (kCachedStorageMaxSize + 1) storages, the first storage
  // must be evicted
  storage = manager->GetStorage(isolation_key);
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));
}

TEST_P(SharedDictionaryManagerTest,
       StorageNotCachedWithModerateMemoryPressure) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment_.RunUntilIdle();

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "p*",
                  {"Hello"});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));

  storage.reset();

  // If `manager` has observed moderate memory pressure, it should not cache the
  // stoarge.
  storage = manager->GetStorage(isolation_key);
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));
}

TEST_P(SharedDictionaryManagerTest,
       StorageNotCachedWithCriticalMemoryPressure) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment_.RunUntilIdle();

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "p*",
                  {"Hello"});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));

  storage.reset();

  // If `manager` has observed critical memory pressure, it should not cache the
  // stoarge.
  storage = manager->GetStorage(isolation_key);
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));
}

TEST_P(SharedDictionaryManagerTest,
       CachedStorageClearedOnModerateMemoryPressure) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "p*",
                  {"Hello"});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));

  storage.reset();

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);
  task_environment_.RunUntilIdle();

  // If `manager` observed moderate memory pressure, it should clear the cached
  // storage.
  storage = manager->GetStorage(isolation_key);
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));
}

TEST_P(SharedDictionaryManagerTest,
       CachedStorageClearedOnCriticalMemoryPressure) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "p*",
                  {"Hello"});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));

  storage.reset();

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment_.RunUntilIdle();

  // If `manager` observed critical memory pressure, it should clear the cached
  // storage.
  storage = manager->GetStorage(isolation_key);
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin1.test/p?")));
}

TEST_P(SharedDictionaryManagerTest, NoWriterForNoUseAsDictionaryHeader) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);

  ASSERT_TRUE(storage);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\n");
  ASSERT_TRUE(headers);
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      GURL("https://origin1.test/testfile.txt"), base::Time::Now(), *headers,
      /*was_fetched_via_cache=*/false,
      /*access_allowed_check_callback=*/base::BindOnce([]() { return true; }));
  EXPECT_FALSE(writer);
}

TEST_P(SharedDictionaryManagerTest, WriterForUseAsDictionaryHeader) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  struct {
    std::string header_string;
    bool expect_success;
  } kTestCases[] = {
      // Empty
      {"", false},

      // Invalid dictionary.
      {"()", false},

      // No `match` value.
      {"dummy", false},

      // Valid `match` value.
      {"match=\"/test\"", true},
      {"match=\"test\"", true},

      // List `match` value is not supported.
      {"match=(\"test1\" \"test2\")", false},
      // Token `match` value is not supported.
      {"match=test", false},

      // Valid `expires` value.
      {"match=\"test\", expires=1000", true},
      // List `expires` value is not supported.
      {"match=\"test\", expires=(1000 2000)", false},
      // String `expires` value is not supported.
      {"match=\"test\", expires=PI", false},

      // Valid `algorithms` value.
      {"match=\"test\", algorithms=sha-256", true},
      {"match=\"test\", algorithms=(sha-256)", true},
      {"match=\"test\", algorithms=(sha-256 sha-512)", true},

      // The sha-256 token must be lowercase.
      // TODO(crbug.com/1413922): Investigate the spec and decide whether to
      // support it or not.
      {"match=\"test\", algorithms=SHA-256", false},

      // Each item in `algorithms` value must be a token.
      {"match=\"test\", algorithms=(\"sha-256\")", false},

      // Unsupported `algorithms` value. We only support sha-256.
      {"match=\"test\", algorithms=(sha-512)", false},

      // We support `raw` type.
      {"match=\"test\", type=raw", true},
      {"match=\"test\", type=(raw)", true},
      // The type must be a token.
      {"match=\"test\", type=\"raw\"", false},
      // We only support `raw` type.
      {"match=\"test\", type=other", false},
      // We don't support multiple types.
      {"match=\"test\", type=(raw,rawx)", false},
  };
  for (const auto& testcase : kTestCases) {
    SCOPED_TRACE(base::StringPrintf("header_string: %s",
                                    testcase.header_string.c_str()));
    scoped_refptr<net::HttpResponseHeaders> headers =
        net::HttpResponseHeaders::TryToCreate(base::StrCat(
            {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
             ": ", testcase.header_string, "\n\n"}));
    ASSERT_TRUE(headers);
    scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
        GURL("https://origin1.test/testfile.txt"), base::Time::Now(), *headers,
        /*was_fetched_via_cache=*/false,
        /*access_allowed_check_callback=*/base::BindOnce([]() {
          return true;
        }));
    EXPECT_EQ(testcase.expect_success, !!writer);
  }
}

TEST_P(SharedDictionaryManagerTest, AccessAllowedCheckReturnTrue) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"test\"\n\n"}));
  ASSERT_TRUE(headers);
  bool callback_called = false;
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      GURL("https://origin1.test/testfile.txt"), base::Time::Now(), *headers,
      /*was_fetched_via_cache=*/false,
      /*access_allowed_check_callback=*/base::BindLambdaForTesting([&]() {
        callback_called = true;
        return true;
      }));
  EXPECT_TRUE(writer);
  EXPECT_TRUE(callback_called);
}

TEST_P(SharedDictionaryManagerTest, AccessAllowedCheckReturnFalse) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"test\"\n\n"}));
  ASSERT_TRUE(headers);
  bool callback_called = false;
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      GURL("https://origin1.test/testfile.txt"), base::Time::Now(), *headers,
      /*was_fetched_via_cache=*/false,
      /*access_allowed_check_callback=*/base::BindLambdaForTesting([&]() {
        callback_called = true;
        return false;
      }));
  EXPECT_FALSE(writer);
  EXPECT_TRUE(callback_called);
}

TEST_P(SharedDictionaryManagerTest, SameDictionaryFromDiskCache) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  GURL dictionary_url = GURL("https://origin1.test/testfile.txt");
  base::Time response_time = base::Time::Now();
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"test\"\n\n"}));
  ASSERT_TRUE(headers);
  scoped_refptr<SharedDictionaryWriter> writer1 = storage->MaybeCreateWriter(
      dictionary_url, response_time, *headers,
      /*was_fetched_via_cache=*/false,
      /*access_allowed_check_callback=*/base::BindLambdaForTesting([&]() {
        return true;
      }));
  ASSERT_TRUE(writer1);
  writer1->Append(kTestData1.c_str(), kTestData1.size());
  writer1->Finish();
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  scoped_refptr<SharedDictionaryWriter> writer2 = storage->MaybeCreateWriter(
      dictionary_url, response_time, *headers,
      /*was_fetched_via_cache=*/true,
      /*access_allowed_check_callback=*/base::BindLambdaForTesting([&]() {
        return true;
      }));
  // MaybeCreateWriter must return false for same dictionary from the disk
  // cache.
  EXPECT_FALSE(writer2);
}

TEST_P(SharedDictionaryManagerTest, DifferentDictionaryFromDiskCache) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  GURL dictionary_url = GURL("https://origin1.test/testfile.txt");
  base::Time response_time = base::Time::Now();
  scoped_refptr<net::HttpResponseHeaders> headers1 =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"test1\"\n\n"}));
  ASSERT_TRUE(headers1);
  scoped_refptr<SharedDictionaryWriter> writer1 = storage->MaybeCreateWriter(
      dictionary_url, response_time, *headers1,
      /*was_fetched_via_cache=*/false,
      /*access_allowed_check_callback=*/base::BindLambdaForTesting([&]() {
        return true;
      }));
  ASSERT_TRUE(writer1);
  writer1->Append(kTestData1.c_str(), kTestData1.size());
  writer1->Finish();
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  scoped_refptr<net::HttpResponseHeaders> headers2 =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"test2\"\n\n"}));
  ASSERT_TRUE(headers1);
  scoped_refptr<SharedDictionaryWriter> writer2 = storage->MaybeCreateWriter(
      dictionary_url, response_time, *headers2,
      /*was_fetched_via_cache=*/true,
      /*access_allowed_check_callback=*/base::BindLambdaForTesting([&]() {
        return true;
      }));
  // The mach value in the header is different, so MaybeCreateWriter() must
  // return a new writer.
  EXPECT_TRUE(writer2);
}

TEST_P(SharedDictionaryManagerTest, WriteAndGetDictionary) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "testfile*",
                  {"hello world"});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  // Check the returned dictionary from GetDictionarySync().
  EXPECT_TRUE(
      storage->GetDictionarySync(GURL("https://origin1.test/testfile")));
  // Different origin.
  EXPECT_FALSE(
      storage->GetDictionarySync(GURL("https://origin2.test/testfile")));
  // No matching dictionary.
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin1.test/test")));
}

TEST_P(SharedDictionaryManagerTest, WriteAndReadDictionary) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  base::Time now_time = base::Time::Now();

  const std::string data1 = "hello ";
  const std::string data2 = "world";
  // Write the test data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "testfile*",
                  {data1, data2});

  // Calculate the hash.
  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  secure_hash->Update(data1.c_str(), data1.size());
  secure_hash->Update(data2.c_str(), data2.size());
  net::SHA256HashValue sha256;
  secure_hash->Finish(sha256.data, sizeof(sha256.data));

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  // Check the returned dictionary from GetDictionarySync().
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionarySync(GURL("https://origin1.test/testfile?hello"));
  ASSERT_TRUE(dict);
  EXPECT_EQ(data1.size() + data2.size(), dict->size());
  EXPECT_EQ(sha256, dict->hash());

  // Read and check the dictionary binary.
  switch (GetParam()) {
    case TestManagerType::kInMemory: {
      EXPECT_EQ(net::OK,
                dict->ReadAll(base::BindOnce([](int rv) { NOTREACHED(); })));
      break;
    }
    case TestManagerType::kOnDisk: {
      base::RunLoop run_loop;
      EXPECT_EQ(net::ERR_IO_PENDING,
                dict->ReadAll(base::BindLambdaForTesting([&](int rv) {
                  EXPECT_EQ(net::OK, rv);
                  run_loop.Quit();
                })));
      run_loop.Run();
      break;
    }
  }

  ASSERT_TRUE(dict->data());
  EXPECT_EQ(data1 + data2, std::string(dict->data()->data(), dict->size()));

  switch (GetParam()) {
    case TestManagerType::kInMemory: {
      // Check the internal state of SharedDictionaryStorageInMemory.
      const auto& dictionary_map = GetInMemoryDictionaryMap(storage.get());
      EXPECT_EQ(1u, dictionary_map.size());
      EXPECT_EQ(url::SchemeHostPort(GURL("https://origin1.test/")),
                dictionary_map.begin()->first);

      EXPECT_EQ(1u, dictionary_map.begin()->second.size());
      EXPECT_EQ("/testfile*", dictionary_map.begin()->second.begin()->first);
      const auto& dictionary_info =
          dictionary_map.begin()->second.begin()->second;
      EXPECT_EQ(GURL("https://origin1.test/dict"), dictionary_info.url());
      EXPECT_EQ(now_time, dictionary_info.response_time());
      EXPECT_EQ(GetDefaultExpiration(), dictionary_info.expiration());
      EXPECT_EQ("/testfile*", dictionary_info.match());
      EXPECT_EQ(data1.size() + data2.size(), dictionary_info.size());
      EXPECT_EQ(data1 + data2, std::string(dictionary_info.data()->data(),
                                           dictionary_info.size()));
      EXPECT_EQ(sha256, dictionary_info.hash());
      break;
    }
    case TestManagerType::kOnDisk: {
      // Check the internal state of SharedDictionaryStorageOnDisk.
      const auto& dictionary_map = GetOnDiskDictionaryMap(storage.get());
      EXPECT_EQ(1u, dictionary_map.size());
      EXPECT_EQ(url::SchemeHostPort(GURL("https://origin1.test/")),
                dictionary_map.begin()->first);

      EXPECT_EQ(1u, dictionary_map.begin()->second.size());
      EXPECT_EQ("/testfile*", dictionary_map.begin()->second.begin()->first);
      const auto& dictionary_info =
          dictionary_map.begin()->second.begin()->second;
      EXPECT_EQ(GURL("https://origin1.test/dict"), dictionary_info.url());
      EXPECT_EQ(now_time, dictionary_info.response_time());
      EXPECT_EQ(GetDefaultExpiration(), dictionary_info.expiration());
      EXPECT_EQ("/testfile*", dictionary_info.match());
      EXPECT_EQ(data1.size() + data2.size(), dictionary_info.size());
      CheckDiskCacheEntryDataEquals(
          static_cast<SharedDictionaryManagerOnDisk*>(manager.get())
              ->disk_cache(),
          dictionary_info.disk_cache_key_token(), data1 + data2);
      EXPECT_EQ(sha256, dictionary_info.hash());
      break;
    }
  }
}
TEST_P(SharedDictionaryManagerTest, OverrideDictionary) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);

  const GURL url1 = GURL("https://origin1.test/dict1");
  const GURL url2 = GURL("https://origin1.test/dict2");
  const std::string match = "path*";
  const std::string data1 = "hello";
  const std::string data2 = "world";
  // Write a test dictionary.
  WriteDictionary(storage.get(), url1, match, {data1});

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  std::vector<network::mojom::SharedDictionaryInfoPtr> result1 =
      GetSharedDictionaryInfo(manager.get(), isolation_key);
  ASSERT_EQ(1u, result1.size());
  EXPECT_EQ(url1, result1[0]->dictionary_url);

  // Write another dictionary with same `match`.
  WriteDictionary(storage.get(), url2, match, {data2});

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  std::vector<network::mojom::SharedDictionaryInfoPtr> result2 =
      GetSharedDictionaryInfo(manager.get(), isolation_key);
  ASSERT_EQ(1u, result2.size());
  EXPECT_EQ(url2, result2[0]->dictionary_url);
}

TEST_P(SharedDictionaryManagerTest, ZeroSizeDictionaryShouldNotBeStored) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  // Write the zero size data to the dictionary.
  WriteDictionary(storage.get(), GURL("https://origin1.test/dict"), "testfile*",
                  {});

  // Check the returned dictionary from GetDictionarySync().
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionarySync(GURL("https://origin1.test/testfile?hello"));
  EXPECT_FALSE(dict);
}

TEST_P(SharedDictionaryManagerTest,
       CacheEvictionSizeExceededOnSetCacheMaxSize) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);

  WriteDictionary(storage.get(), GURL("https://origin1.test/d1"), "p1*",
                  {kTestData1});
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage.get(), GURL("https://origin2.test/d2"), "p2*",
                  {kTestData1});
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage.get(), GURL("https://origin3.test/d1"), "p3*",
                  {kTestData1});

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  task_environment_.FastForwardBy(base::Seconds(1));

  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 2);

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin1.test/p1?")));
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin2.test/p2?")));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin3.test/p3?")));
}

TEST_P(SharedDictionaryManagerTest, CacheEvictionZeroMaxSizeCountExceeded) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  std::vector<scoped_refptr<SharedDictionaryStorage>> storages;
  for (size_t i = 0; i < kCacheMaxCount; ++i) {
    net::SharedDictionaryIsolationKey isolation_key(
        url::Origin::Create(
            GURL(base::StringPrintf("https://origind%03" PRIuS ".test", i))),
        net::SchemefulSite(
            GURL(base::StringPrintf("https://origind%03" PRIuS ".test", i))));

    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    storages.push_back(storage);

    WriteDictionary(
        storage.get(),
        GURL(base::StringPrintf("https://origin.test/d%03" PRIuS, i)),
        base::StringPrintf("p%03" PRIuS, i), {kTestData1});
    if (GetParam() == TestManagerType::kOnDisk) {
      FlushCacheTasks();
    }
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  for (size_t i = 0; i < kCacheMaxCount; ++i) {
    EXPECT_TRUE(storages[i]->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Write one more dictionary. The total count exceeds the limit.
  {
    net::SharedDictionaryIsolationKey isolation_key(
        url::Origin::Create(GURL(base::StringPrintf(
            "https://origind%03" PRIuS ".test", kCacheMaxCount))),
        net::SchemefulSite(GURL(base::StringPrintf(
            "https://origind%03" PRIuS ".test", kCacheMaxCount))));
    scoped_refptr<SharedDictionaryStorage> storage =
        manager->GetStorage(isolation_key);
    storages.push_back(storage);
    WriteDictionary(storage.get(),
                    GURL(base::StringPrintf("https://origin.test/d%03" PRIuS,
                                            kCacheMaxCount)),
                    base::StringPrintf("p%03" PRIuS, kCacheMaxCount),
                    {kTestData1});
    if (GetParam() == TestManagerType::kOnDisk) {
      FlushCacheTasks();
    }
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Old dictionaries must be deleted until the total count reaches
  // kCacheMaxCount * 0.9.
  for (size_t i = 0; i < kCacheMaxCount - kCacheMaxCount * 0.9; ++i) {
    EXPECT_FALSE(storages[i]->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
  }

  // Newer dictionaries must not be deleted.
  for (size_t i = kCacheMaxCount - kCacheMaxCount * 0.9 + 1;
       i <= kCacheMaxCount; ++i) {
    EXPECT_TRUE(storages[i]->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
  }
}

TEST_P(SharedDictionaryManagerTest,
       CacheEvictionOnNewDictionaryMultiIsolation) {
  net::SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                   kSite1);
  net::SharedDictionaryIsolationKey isolation_key2(url::Origin::Create(kUrl2),
                                                   kSite2);
  net::SharedDictionaryIsolationKey isolation_key3(url::Origin::Create(kUrl3),
                                                   kSite3);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 2);
  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  ASSERT_TRUE(storage1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  ASSERT_TRUE(storage2);
  scoped_refptr<SharedDictionaryStorage> storage3 =
      manager->GetStorage(isolation_key3);
  ASSERT_TRUE(storage3);

  WriteDictionary(storage1.get(), GURL("https://origin1.test/d1"), "p1*",
                  {kTestData1});
  WriteDictionary(storage2.get(), GURL("https://origin2.test/d2"), "p2*",
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  EXPECT_TRUE(storage1->GetDictionarySync(GURL("https://origin1.test/p1?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p2?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage3.get(), GURL("https://origin3.test/d1"), "p3*",
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  EXPECT_FALSE(storage1->GetDictionarySync(GURL("https://origin1.test/p1?")));
  EXPECT_FALSE(storage2->GetDictionarySync(GURL("https://origin2.test/p2?")));
  EXPECT_TRUE(storage3->GetDictionarySync(GURL("https://origin3.test/p3?")));
}

TEST_P(SharedDictionaryManagerTest, CacheEvictionAfterUpdatingLastUsedTime) {
  net::SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                   kSite1);
  net::SharedDictionaryIsolationKey isolation_key2(url::Origin::Create(kUrl2),
                                                   kSite2);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  ASSERT_TRUE(storage1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  ASSERT_TRUE(storage2);

  // Dictionary 1-1.
  WriteDictionary(storage1.get(), GURL("https://origin1.test/d1"), "p1*",
                  {kTestData1});
  task_environment_.FastForwardBy(base::Seconds(1));
  // Dictionary 1-2.
  WriteDictionary(storage1.get(), GURL("https://origin1.test/d2"), "p2*",
                  {kTestData1});
  task_environment_.FastForwardBy(base::Seconds(1));
  // Dictionary 2-1.
  WriteDictionary(storage2.get(), GURL("https://origin2.test/d1"), "p1*",
                  {kTestData1});
  task_environment_.FastForwardBy(base::Seconds(1));
  // Dictionary 2-2.
  WriteDictionary(storage2.get(), GURL("https://origin2.test/d2"), "p2*",
                  {kTestData1});

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  task_environment_.FastForwardBy(base::Seconds(1));

  // Call GetDictionary to update the last used time of the dictionary 1-1.
  std::unique_ptr<SharedDictionary> dict1 =
      storage1->GetDictionarySync(GURL("https://origin1.test/p1?"));
  ASSERT_TRUE(dict1);

  // Set the max size to kTestData1.size() * 3. The low water mark will be
  // kTestData1.size() * 2.7 (3 * 0.9).
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 3);

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage1->GetDictionarySync(GURL("https://origin1.test/p1?")));
  EXPECT_FALSE(storage1->GetDictionarySync(GURL("https://origin1.test/p2?")));
  EXPECT_FALSE(storage2->GetDictionarySync(GURL("https://origin2.test/p1?")));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p2?")));
}

TEST_P(SharedDictionaryManagerTest, CacheEvictionPerSiteSizeExceeded) {
  net::SharedDictionaryIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                   kSite1);
  net::SharedDictionaryIsolationKey isolation_key2(url::Origin::Create(kUrl1),
                                                   kSite2);
  net::SharedDictionaryIsolationKey isolation_key3(url::Origin::Create(kUrl2),
                                                   kSite1);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  // The size limit per site is kTestData1.size() * 4 / 2.
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 4);

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  scoped_refptr<SharedDictionaryStorage> storage3 =
      manager->GetStorage(isolation_key3);

  WriteDictionary(storage1.get(), GURL("https://origin1.test/d"), "p*",
                  {kTestData1});
  WriteDictionary(storage2.get(), GURL("https://origin2.test/d"), "p*",
                  {kTestData1});
  WriteDictionary(storage3.get(), GURL("https://origin3.test/d"), "p*",
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  EXPECT_TRUE(storage1->GetDictionarySync(GURL("https://origin1.test/p?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(storage3->GetDictionarySync(GURL("https://origin3.test/p?")));
  task_environment_.FastForwardBy(base::Seconds(1));

  WriteDictionary(storage1.get(), GURL("https://origin4.test/d"), "p*",
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  EXPECT_FALSE(storage1->GetDictionarySync(GURL("https://origin1.test/p?")));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p?")));
  EXPECT_TRUE(storage3->GetDictionarySync(GURL("https://origin3.test/p?")));
  EXPECT_TRUE(storage1->GetDictionarySync(GURL("https://origin4.test/p?")));
}

TEST_P(SharedDictionaryManagerTest,
       CacheEvictionPerSiteZeroMaxSizeCountExceeded) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  size_t cache_max_count_per_site = kCacheMaxCount / 2;
  for (size_t i = 0; i < cache_max_count_per_site; ++i) {
    WriteDictionary(
        storage.get(),
        GURL(base::StringPrintf("https://origin.test/d%03" PRIuS, i)),
        base::StringPrintf("p%03" PRIuS, i), {kTestData1});
    if (GetParam() == TestManagerType::kOnDisk) {
      FlushCacheTasks();
    }
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  for (size_t i = 0; i < cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Write one more dictionary. The total count exceeds the limit.
  WriteDictionary(storage.get(),
                  GURL(base::StringPrintf("https://origin.test/d%03" PRIuS,
                                          cache_max_count_per_site)),
                  base::StringPrintf("p%03" PRIuS, cache_max_count_per_site),
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p000?")));

  // Newer dictionaries must not be evicted.
  for (size_t i = 1; i <= cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
  }
}

TEST_P(SharedDictionaryManagerTest,
       CacheEvictionPerSiteNonZeroMaxSizeCountExceeded) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() *
                           kCacheMaxCount);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  size_t cache_max_count_per_site = kCacheMaxCount / 2;
  for (size_t i = 0; i < cache_max_count_per_site; ++i) {
    WriteDictionary(
        storage.get(),
        GURL(base::StringPrintf("https://origin.test/d%03" PRIuS, i)),
        base::StringPrintf("p%03" PRIuS, i), {kTestData1});
    if (GetParam() == TestManagerType::kOnDisk) {
      FlushCacheTasks();
    }
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  for (size_t i = 0; i < cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Write one more dictionary. The total count exceeds the limit.
  WriteDictionary(storage.get(),
                  GURL(base::StringPrintf("https://origin.test/d%03" PRIuS,
                                          cache_max_count_per_site)),
                  base::StringPrintf("p%03" PRIuS, cache_max_count_per_site),
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p000?")));

  // Newer dictionaries must not be evicted.
  for (size_t i = 1; i <= cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
  }
}

TEST_P(SharedDictionaryManagerTest,
       CacheEvictionPerSiteBothSizeAndCountExceeded) {
  net::SharedDictionaryIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                  kSite1);

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() *
                           kCacheMaxCount);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  size_t cache_max_count_per_site = kCacheMaxCount / 2;
  for (size_t i = 0; i < cache_max_count_per_site; ++i) {
    WriteDictionary(
        storage.get(),
        GURL(base::StringPrintf("https://origin.test/d%03" PRIuS, i)),
        base::StringPrintf("p%03" PRIuS, i), {kTestData1});
    if (GetParam() == TestManagerType::kOnDisk) {
      FlushCacheTasks();
    }
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  for (size_t i = 0; i < cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Write one more dictionary. Both the total size and count exceeds the limit.
  WriteDictionary(storage.get(),
                  GURL(base::StringPrintf("https://origin.test/d%03" PRIuS,
                                          cache_max_count_per_site)),
                  base::StringPrintf("p%03" PRIuS, cache_max_count_per_site),
                  {kTestData1, kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  task_environment_.FastForwardBy(base::Seconds(1));

  // The last dictionary size is kTestData1.size() * 2. So the oldest two
  // dictionaries must be evicted.
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p000?")));
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p001?")));

  // Newer dictionaries must not be deleted.
  for (size_t i = 2; i <= cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionarySync(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
  }
}

TEST_P(SharedDictionaryManagerTest, ClearDataMatchFrameOrigin) {
  net::SharedDictionaryIsolationKey isolation_key(
      url::Origin::Create(GURL("https://target.test/")),
      net::SchemefulSite(GURL("https://top-frame.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  WriteDictionary(storage.get(), GURL("https://origin.test/1"), "p1*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/2"), "p2*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/3"), "p3*",
                  {kTestData1});
  // Move the clock forward by 12 hours.
  task_environment_.FastForwardBy(base::Hours(12));

  base::RunLoop run_loop;
  manager->ClearData(base::Time::Now() - base::Days(2),
                     base::Time::Now() - base::Days(1),
                     base::BindRepeating([](const GURL& url) {
                       return url == GURL("https://target.test/");
                     }),
                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p3?")));
}

TEST_P(SharedDictionaryManagerTest, ClearDataMatchTopFrameSite) {
  net::SharedDictionaryIsolationKey isolation_key(
      url::Origin::Create(GURL("https://frame.test/")),
      net::SchemefulSite(GURL("https://target.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  WriteDictionary(storage.get(), GURL("https://origin.test/1"), "p1*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/2"), "p2*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/3"), "p3*",
                  {kTestData1});
  // Move the clock forward by 12 hours.
  task_environment_.FastForwardBy(base::Hours(12));

  base::RunLoop run_loop;
  manager->ClearData(base::Time::Now() - base::Days(2),
                     base::Time::Now() - base::Days(1),
                     base::BindRepeating([](const GURL& url) {
                       return url == GURL("https://target.test/");
                     }),
                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p3?")));
}

TEST_P(SharedDictionaryManagerTest, ClearDataMatchDictionaryUrl) {
  net::SharedDictionaryIsolationKey isolation_key(
      url::Origin::Create(GURL("https://frame.test/")),
      net::SchemefulSite(GURL("https://top-frame.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  WriteDictionary(storage.get(), GURL("https://target.test/1"), "p1*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://target.test/2"), "p2*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://target.test/3"), "p3*",
                  {kTestData1});
  // Move the clock forward by 12 hours.
  task_environment_.FastForwardBy(base::Hours(12));

  base::RunLoop run_loop;
  manager->ClearData(base::Time::Now() - base::Days(2),
                     base::Time::Now() - base::Days(1),
                     base::BindRepeating([](const GURL& url) {
                       return url == GURL("https://target.test/");
                     }),
                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://target.test/p1?")));
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://target.test/p2?")));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://target.test/p3?")));
}

TEST_P(SharedDictionaryManagerTest, ClearDataNullUrlMatcher) {
  net::SharedDictionaryIsolationKey isolation_key(
      url::Origin::Create(GURL("https://frame.test/")),
      net::SchemefulSite(GURL("https://top-frame.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  WriteDictionary(storage.get(), GURL("https://origin.test/1"), "p1*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/2"), "p2*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/3"), "p3*",
                  {kTestData1});
  // Move the clock forward by 12 hours.
  task_environment_.FastForwardBy(base::Hours(12));

  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p3?")));
}

TEST_P(SharedDictionaryManagerTest, ClearDataDoNotInvalidateActiveDictionary) {
  net::SharedDictionaryIsolationKey isolation_key(
      url::Origin::Create(GURL("https://frame.test/")),
      net::SchemefulSite(GURL("https://top-frame.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  WriteDictionary(storage.get(), GURL("https://origin.test/1"), "p1*",
                  {kTestData1});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/2"), "p2*",
                  {kTestData2});
  // Move the clock forward by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  WriteDictionary(storage.get(), GURL("https://origin.test/3"), "p3*",
                  {kTestData1});
  // Move the clock forward by 12 hours.
  task_environment_.FastForwardBy(base::Hours(12));

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  // Get a dictionary before calling ClearData().
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionarySync(GURL("https://origin.test/p2?"));
  ASSERT_TRUE(dict);

  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p3?")));

  // We can still read the deleted dictionary from `dict`.
  net::TestCompletionCallback read_callback;
  EXPECT_EQ(net::OK,
            read_callback.GetResult(dict->ReadAll(read_callback.callback())));
  EXPECT_EQ(kTestData2,
            std::string(reinterpret_cast<const char*>(dict->data()->data()),
                        dict->size()));
}

TEST_P(SharedDictionaryManagerTest, ClearDataForIsolationKey) {
  net::SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://frame1.test/")),
      net::SchemefulSite(GURL("https://target1.test")));
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://frame2.test/")),
      net::SchemefulSite(GURL("https://target2.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  WriteDictionary(storage1.get(), GURL("https://origin1.test/1"), "p1*",
                  {kTestData1});
  WriteDictionary(storage1.get(), GURL("https://origin1.test/2"), "p2*",
                  {kTestData1});

  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  WriteDictionary(storage2.get(), GURL("https://origin2.test/1"), "p1*",
                  {kTestData1});
  WriteDictionary(storage2.get(), GURL("https://origin2.test/2"), "p2*",
                  {kTestData1});

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage1->GetDictionarySync(GURL("https://origin1.test/p1?")));
  EXPECT_TRUE(storage1->GetDictionarySync(GURL("https://origin1.test/p2?")));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p1?")));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p2?")));

  base::RunLoop run_loop;
  manager->ClearDataForIsolationKey(isolation_key1, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(storage1->GetDictionarySync(GURL("https://origin1.test/p1?")));
  EXPECT_FALSE(storage1->GetDictionarySync(GURL("https://origin1.test/p2?")));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p1?")));
  EXPECT_TRUE(storage2->GetDictionarySync(GURL("https://origin2.test/p2?")));
}

TEST_P(SharedDictionaryManagerTest, GetUsageInfo) {
  net::SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://frame1.test/")),
      net::SchemefulSite(GURL("https://target1.test")));
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://frame2.test/")),
      net::SchemefulSite(GURL("https://target2.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  WriteDictionary(storage1.get(), GURL("https://origin1.test/1"), "p1*",
                  {kTestData1});
  WriteDictionary(storage1.get(), GURL("https://origin1.test/2"), "p2*",
                  {kTestData2});

  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  WriteDictionary(storage2.get(), GURL("https://origin2.test/1"), "p1*",
                  {kTestData2});
  WriteDictionary(storage2.get(), GURL("https://origin2.test/2"), "p2*",
                  {kTestData2});

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  base::RunLoop run_loop;
  manager->GetUsageInfo(base::BindLambdaForTesting(
      [&](const std::vector<net::SharedDictionaryUsageInfo>& usage_info) {
        EXPECT_THAT(
            usage_info,
            UnorderedElementsAreArray(
                {net::SharedDictionaryUsageInfo{
                     .isolation_key = isolation_key1,
                     .total_size_bytes = kTestData1.size() + kTestData2.size()},
                 net::SharedDictionaryUsageInfo{
                     .isolation_key = isolation_key2,
                     .total_size_bytes = kTestData2.size() * 2}}));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_P(SharedDictionaryManagerTest, GetUsageInfoEmptyResult) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  base::RunLoop run_loop;
  manager->GetUsageInfo(base::BindLambdaForTesting(
      [&](const std::vector<net::SharedDictionaryUsageInfo>& usage_info) {
        EXPECT_TRUE(usage_info.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_P(SharedDictionaryManagerTest, GetSharedDictionaryInfo) {
  net::SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://frame1.test/")),
      net::SchemefulSite(GURL("https://target1.test")));
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://frame2.test/")),
      net::SchemefulSite(GURL("https://target2.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  const base::Time start_time = base::Time::Now();

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  WriteDictionary(storage1.get(), GURL("https://origin1.test/1"), "p1*",
                  {kTestData1});

  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage1.get(), GURL("https://origin1.test/2"), "p2*",
                  {kTestData2});

  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage2.get(), GURL("https://origin2.test/d"), "p*",
                  {kTestData2}, /*additional_options=*/",expires=123456");

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  task_environment_.FastForwardBy(base::Seconds(1));
  // Update `last_used_time`.
  EXPECT_TRUE(storage1->GetDictionarySync(GURL("https://origin1.test/p2?")));

  std::vector<network::mojom::SharedDictionaryInfoPtr> result1 =
      GetSharedDictionaryInfo(manager.get(), isolation_key1);
  ASSERT_EQ(2u, result1.size());

  EXPECT_EQ("/p1*", result1[0]->match);
  EXPECT_EQ(GURL("https://origin1.test/1"), result1[0]->dictionary_url);
  EXPECT_EQ(start_time, result1[0]->response_time);
  EXPECT_EQ(GetDefaultExpiration(), result1[0]->expiration);
  EXPECT_EQ(start_time, result1[0]->last_used_time);
  EXPECT_EQ(kTestData1.size(), result1[0]->size);
  EXPECT_EQ(kTestData1Hash, result1[0]->hash);

  EXPECT_EQ("/p2*", result1[1]->match);
  EXPECT_EQ(GURL("https://origin1.test/2"), result1[1]->dictionary_url);
  EXPECT_EQ(start_time + base::Seconds(1), result1[1]->response_time);
  EXPECT_EQ(GetDefaultExpiration(), result1[1]->expiration);
  EXPECT_EQ(start_time + base::Seconds(3), result1[1]->last_used_time);
  EXPECT_EQ(kTestData2.size(), result1[1]->size);
  EXPECT_EQ(kTestData2Hash, result1[1]->hash);

  std::vector<network::mojom::SharedDictionaryInfoPtr> result2 =
      GetSharedDictionaryInfo(manager.get(), isolation_key2);
  ASSERT_EQ(1u, result2.size());
  EXPECT_EQ("/p*", result2[0]->match);
  EXPECT_EQ(GURL("https://origin2.test/d"), result2[0]->dictionary_url);
  EXPECT_EQ(start_time + base::Seconds(2), result2[0]->response_time);
  EXPECT_EQ(base::Seconds(123456), result2[0]->expiration);
  EXPECT_EQ(start_time + base::Seconds(2), result2[0]->last_used_time);
  EXPECT_EQ(kTestData2.size(), result2[0]->size);
  EXPECT_EQ(kTestData2Hash, result2[0]->hash);
}

TEST_P(SharedDictionaryManagerTest, GetSharedDictionaryInfoEmptyResult) {
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();
  EXPECT_TRUE(GetSharedDictionaryInfo(
                  manager.get(),
                  net::SharedDictionaryIsolationKey(
                      url::Origin::Create(GURL("https://frame.test/")),
                      net::SchemefulSite(GURL("https://top-frame.test"))))
                  .empty());
}

TEST_P(SharedDictionaryManagerTest, GetTotalSizeAndOrigins) {
  net::SharedDictionaryIsolationKey isolation_key1(
      url::Origin::Create(GURL("https://frame1.test/")),
      net::SchemefulSite(GURL("https://target1.test")));
  net::SharedDictionaryIsolationKey isolation_key2(
      url::Origin::Create(GURL("https://frame2.test/")),
      net::SchemefulSite(GURL("https://target2.test")));
  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  const base::Time start_time = base::Time::Now();

  scoped_refptr<SharedDictionaryStorage> storage1 =
      manager->GetStorage(isolation_key1);
  WriteDictionary(storage1.get(), GURL("https://origin1.test/1"), "p1*",
                  {kTestData1});

  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage1.get(), GURL("https://origin1.test/2"), "p2*",
                  {kTestData2});

  scoped_refptr<SharedDictionaryStorage> storage2 =
      manager->GetStorage(isolation_key2);
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage2.get(), GURL("https://origin2.test/d"), "p*",
                  {kTestData2});

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(GetOriginsBetween(manager.get(), start_time - base::Seconds(1),
                                start_time)
                  .empty());

  EXPECT_THAT(GetOriginsBetween(manager.get(), start_time,
                                start_time + base::Seconds(1)),
              testing::ElementsAreArray({isolation_key1.frame_origin()}));

  EXPECT_THAT(
      GetOriginsBetween(manager.get(), start_time,
                        start_time + base::Seconds(3)),
      testing::UnorderedElementsAreArray(
          {isolation_key1.frame_origin(), isolation_key2.frame_origin()}));
}

TEST_P(SharedDictionaryManagerTest, DeleteExpiredDictionariesOnGetDictionary) {
  net::SharedDictionaryIsolationKey isolation_key(
      url::Origin::Create(GURL("https://frame.test/")),
      net::SchemefulSite(GURL("https://target.test")));

  std::unique_ptr<SharedDictionaryManager> manager =
      CreateSharedDictionaryManager();

  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  WriteDictionary(storage.get(), GURL("https://origin.test/d1"), "p1*",
                  {kTestData1}, /*additional_options=*/",expires=20");

  task_environment_.FastForwardBy(base::Seconds(10));

  WriteDictionary(storage.get(), GURL("https://origin.test/d1"), "p2*",
                  {kTestData2}, /*additional_options=*/",expires=5");

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  task_environment_.FastForwardBy(base::Seconds(4));

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p2?")));

  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));

  EXPECT_EQ(2u, GetSharedDictionaryInfo(manager.get(), isolation_key).size());
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p2?")));
  EXPECT_EQ(1u, GetSharedDictionaryInfo(manager.get(), isolation_key).size());

  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1u, GetSharedDictionaryInfo(manager.get(), isolation_key).size());
  EXPECT_FALSE(storage->GetDictionarySync(GURL("https://origin.test/p1?")));
  EXPECT_TRUE(GetSharedDictionaryInfo(manager.get(), isolation_key).empty());
}

}  // namespace network
