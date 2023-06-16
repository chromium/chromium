// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
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
#include "net/http/http_response_headers.h"
#include "services/network/shared_dictionary/shared_dictionary.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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
                     base::Time now_time = base::Time::Now()) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"/", match, "\"\n\n"}));
  ASSERT_TRUE(headers);
  scoped_refptr<SharedDictionaryWriter> writer =
      storage->MaybeCreateWriter(dictionary_url, now_time, *headers);
  ASSERT_TRUE(writer);
  for (const std::string& data : data_list) {
    writer->Append(data.c_str(), data.size());
  }
  writer->Finish();
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
            /*app_status_listener=*/nullptr,
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
      GURL("https://origin1.test/testfile.txt"), base::Time::Now(), *headers);
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
        GURL("https://origin1.test/testfile.txt"), base::Time::Now(), *headers);
    EXPECT_EQ(testcase.expect_success, !!writer);
  }
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

  // Check the returned dictionary from GetDictionary().
  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin1.test/testfile")));
  // Different origin.
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin2.test/testfile")));
  // No matching dictionary.
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin1.test/test")));
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
                  {data1, data2}, now_time);

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

  // Check the returned dictionary from GetDictionary().
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionary(GURL("https://origin1.test/testfile?hello"));
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
      EXPECT_EQ(shared_dictionary::kDefaultExpiration,
                dictionary_info.expiration());
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
      EXPECT_EQ(shared_dictionary::kDefaultExpiration,
                dictionary_info.expiration());
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

  // Check the returned dictionary from GetDictionary().
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionary(GURL("https://origin1.test/testfile?hello"));
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

  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin1.test/p1?")));
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin2.test/p2?")));
  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin3.test/p3?")));
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
    EXPECT_TRUE(storages[i]->GetDictionary(
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
    EXPECT_FALSE(storages[i]->GetDictionary(
        GURL(base::StringPrintf("https://origin.test/p%03" PRIuS "?", i))));
  }

  // Newer dictionaries must not be deleted.
  for (size_t i = kCacheMaxCount - kCacheMaxCount * 0.9 + 1;
       i <= kCacheMaxCount; ++i) {
    EXPECT_TRUE(storages[i]->GetDictionary(
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
  EXPECT_TRUE(storage1->GetDictionary(GURL("https://origin1.test/p1?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(storage2->GetDictionary(GURL("https://origin2.test/p2?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  WriteDictionary(storage3.get(), GURL("https://origin3.test/d1"), "p3*",
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  EXPECT_FALSE(storage1->GetDictionary(GURL("https://origin1.test/p1?")));
  EXPECT_FALSE(storage2->GetDictionary(GURL("https://origin2.test/p2?")));
  EXPECT_TRUE(storage3->GetDictionary(GURL("https://origin3.test/p3?")));
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
      storage1->GetDictionary(GURL("https://origin1.test/p1?"));
  ASSERT_TRUE(dict1);

  // Set the max size to kTestData1.size() * 3. The low water mark will be
  // kTestData1.size() * 2.7 (3 * 0.9).
  manager->SetCacheMaxSize(/*cache_max_size=*/kTestData1.size() * 3);

  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }

  EXPECT_TRUE(storage1->GetDictionary(GURL("https://origin1.test/p1?")));
  EXPECT_FALSE(storage1->GetDictionary(GURL("https://origin1.test/p2?")));
  EXPECT_FALSE(storage2->GetDictionary(GURL("https://origin2.test/p1?")));
  EXPECT_TRUE(storage2->GetDictionary(GURL("https://origin2.test/p2?")));
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
  EXPECT_TRUE(storage1->GetDictionary(GURL("https://origin1.test/p?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(storage2->GetDictionary(GURL("https://origin2.test/p?")));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(storage3->GetDictionary(GURL("https://origin3.test/p?")));
  task_environment_.FastForwardBy(base::Seconds(1));

  WriteDictionary(storage1.get(), GURL("https://origin4.test/d"), "p*",
                  {kTestData1});
  if (GetParam() == TestManagerType::kOnDisk) {
    FlushCacheTasks();
  }
  EXPECT_FALSE(storage1->GetDictionary(GURL("https://origin1.test/p?")));
  EXPECT_TRUE(storage2->GetDictionary(GURL("https://origin2.test/p?")));
  EXPECT_TRUE(storage3->GetDictionary(GURL("https://origin3.test/p?")));
  EXPECT_TRUE(storage1->GetDictionary(GURL("https://origin4.test/p?")));
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
    EXPECT_TRUE(storage->GetDictionary(
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

  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p000?")));

  // Newer dictionaries must not be evicted.
  for (size_t i = 1; i <= cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionary(
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
    EXPECT_TRUE(storage->GetDictionary(
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

  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p000?")));

  // Newer dictionaries must not be evicted.
  for (size_t i = 1; i <= cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionary(
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
    EXPECT_TRUE(storage->GetDictionary(
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
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p000?")));
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p001?")));

  // Newer dictionaries must not be deleted.
  for (size_t i = 2; i <= cache_max_count_per_site; ++i) {
    EXPECT_TRUE(storage->GetDictionary(
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

  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p3?")));
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

  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p3?")));
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

  EXPECT_TRUE(storage->GetDictionary(GURL("https://target.test/p1?")));
  EXPECT_FALSE(storage->GetDictionary(GURL("https://target.test/p2?")));
  EXPECT_TRUE(storage->GetDictionary(GURL("https://target.test/p3?")));
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

  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p3?")));
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
      storage->GetDictionary(GURL("https://origin.test/p2?"));
  ASSERT_TRUE(dict);

  base::RunLoop run_loop;
  manager->ClearData(
      base::Time::Now() - base::Days(2), base::Time::Now() - base::Days(1),
      base::RepeatingCallback<bool(const GURL&)>(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p1?")));
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin.test/p2?")));
  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin.test/p3?")));

  // We can still read the deleted dictionary from `dict`.
  net::TestCompletionCallback read_callback;
  EXPECT_EQ(net::OK,
            read_callback.GetResult(dict->ReadAll(read_callback.callback())));
  EXPECT_EQ(kTestData2,
            std::string(reinterpret_cast<const char*>(dict->data()->data()),
                        dict->size()));
}

}  // namespace network
