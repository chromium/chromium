// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "services/network/shared_dictionary/shared_dictionary.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {
const GURL kUrl1("https://origin1.test/");
const GURL kUrl2("https://origin2.test/");
const net::SchemefulSite kSite1(kUrl1);
const net::SchemefulSite kSite2(kUrl2);
}  // namespace

class SharedDictionaryManagerTest : public ::testing::Test {
 public:
  SharedDictionaryManagerTest() = default;
  ~SharedDictionaryManagerTest() override = default;

  SharedDictionaryManagerTest(const SharedDictionaryManagerTest&) = delete;
  SharedDictionaryManagerTest& operator=(const SharedDictionaryManagerTest&) =
      delete;

 protected:
  const std::map<
      url::Origin,
      std::map<std::string, SharedDictionaryStorageInMemory::DictionaryInfo>>&
  GetInMemoryDictionaryMap(SharedDictionaryStorage* storage) {
    return static_cast<SharedDictionaryStorageInMemory*>(storage)
        ->GetDictionaryMapForTesting();
  }
};

TEST_F(SharedDictionaryManagerTest, SameStorageForSameIsolationKey) {
  std::unique_ptr<SharedDictionaryManager> manager =
      SharedDictionaryManager::CreateInMemory();

  SharedDictionaryStorageIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                     kSite1);
  SharedDictionaryStorageIsolationKey isolation_key2(url::Origin::Create(kUrl1),
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

TEST_F(SharedDictionaryManagerTest, DifferentStorageForDifferentIsolationKey) {
  std::unique_ptr<SharedDictionaryManager> manager =
      SharedDictionaryManager::CreateInMemory();

  SharedDictionaryStorageIsolationKey isolation_key1(url::Origin::Create(kUrl1),
                                                     kSite1);
  SharedDictionaryStorageIsolationKey isolation_key2(url::Origin::Create(kUrl2),
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

TEST_F(SharedDictionaryManagerTest, NoWriterForNoUseAsDictionaryHeader) {
  std::unique_ptr<SharedDictionaryManager> manager =
      SharedDictionaryManager::CreateInMemory();

  SharedDictionaryStorageIsolationKey isolation_key(url::Origin::Create(kUrl1),
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

TEST_F(SharedDictionaryManagerTest, WriterForUseAsDictionaryHeader) {
  std::unique_ptr<SharedDictionaryManager> manager =
      SharedDictionaryManager::CreateInMemory();

  SharedDictionaryStorageIsolationKey isolation_key(url::Origin::Create(kUrl1),
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

TEST_F(SharedDictionaryManagerTest, WriteAndGetDictionary) {
  std::unique_ptr<SharedDictionaryManager> manager =
      SharedDictionaryManager::CreateInMemory();
  SharedDictionaryStorageIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                    kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  ASSERT_TRUE(storage);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"/testfile*\"\n\n"}));
  ASSERT_TRUE(headers);

  // Write the test data to the dictionary.
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      GURL("https://origin1.test/dict"), base::Time::Now(), *headers);
  ASSERT_TRUE(writer);
  const std::string data = "hello world";
  writer->Append(data.c_str(), data.size());
  writer->Finish();

  // Check the returned dictionary from GetDictionary().
  EXPECT_TRUE(storage->GetDictionary(GURL("https://origin1.test/testfile")));
  // Different origin.
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin2.test/testfile")));
  // No matching dictionary.
  EXPECT_FALSE(storage->GetDictionary(GURL("https://origin1.test/test")));
}

TEST_F(SharedDictionaryManagerTest, WriteAndReadDictionary) {
  std::unique_ptr<SharedDictionaryManager> manager =
      SharedDictionaryManager::CreateInMemory();
  SharedDictionaryStorageIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                    kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"/testfile*\"\n\n"}));
  ASSERT_TRUE(headers);
  base::Time now_time = base::Time::Now();

  // Write the test data to the dictionary.
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      GURL("https://origin1.test/dict"), now_time, *headers);
  ASSERT_TRUE(writer);
  const std::string data1 = "hello ";
  const std::string data2 = "world";
  writer->Append(data1.c_str(), data1.size());
  writer->Append(data2.c_str(), data2.size());
  writer->Finish();

  // Calculate the hash.
  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  secure_hash->Update(data1.c_str(), data1.size());
  secure_hash->Update(data2.c_str(), data2.size());
  net::SHA256HashValue sha256;
  secure_hash->Finish(sha256.data, sizeof(sha256.data));

  // Check the returned dictionary from GetDictionary().
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionary(GURL("https://origin1.test/testfile?hello"));
  ASSERT_TRUE(dict);
  EXPECT_EQ(data1.size() + data2.size(), dict->size());
  EXPECT_EQ(sha256, dict->hash());

  // Read and check the dictionary binary.
  EXPECT_EQ(net::OK,
            dict->ReadAll(base::BindOnce([](int rv) { NOTREACHED(); })));
  ASSERT_TRUE(dict->data());
  EXPECT_EQ(data1 + data2, std::string(dict->data()->data(), dict->size()));

  // Check the internal state of SharedDictionaryStorageInMemory.
  const auto& dictionary_map = GetInMemoryDictionaryMap(storage.get());
  EXPECT_EQ(1u, dictionary_map.size());
  EXPECT_EQ(url::Origin::Create(GURL("https://origin1.test/")),
            dictionary_map.begin()->first);

  EXPECT_EQ(1u, dictionary_map.begin()->second.size());
  EXPECT_EQ("/testfile*", dictionary_map.begin()->second.begin()->first);
  const auto& dictionary_info = dictionary_map.begin()->second.begin()->second;
  EXPECT_EQ(GURL("https://origin1.test/dict"), dictionary_info.url());
  EXPECT_EQ(now_time, dictionary_info.response_time());
  EXPECT_EQ(shared_dictionary::kDefaultExpiration,
            dictionary_info.expiration());
  EXPECT_EQ("/testfile*", dictionary_info.path_pattern());
  EXPECT_EQ(data1.size() + data2.size(), dictionary_info.size());
  EXPECT_EQ(data1 + data2, std::string(dictionary_info.data()->data(),
                                       dictionary_info.size()));
  EXPECT_EQ(sha256, dictionary_info.hash());
}

TEST_F(SharedDictionaryManagerTest, ZeroSizeDictionaryShouldNotBeStored) {
  std::unique_ptr<SharedDictionaryManager> manager =
      SharedDictionaryManager::CreateInMemory();
  SharedDictionaryStorageIsolationKey isolation_key(url::Origin::Create(kUrl1),
                                                    kSite1);
  scoped_refptr<SharedDictionaryStorage> storage =
      manager->GetStorage(isolation_key);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(base::StrCat(
          {"HTTP/1.1 200 OK\n", shared_dictionary::kUseAsDictionaryHeaderName,
           ": match=\"/testfile*\"\n\n"}));
  ASSERT_TRUE(headers);
  base::Time now_time = base::Time::Now();

  // Write the zero size data to the dictionary.
  scoped_refptr<SharedDictionaryWriter> writer = storage->MaybeCreateWriter(
      GURL("https://origin1.test/dict"), now_time, *headers);
  ASSERT_TRUE(writer);
  writer->Finish();

  // Check the returned dictionary from GetDictionary().
  std::unique_ptr<SharedDictionary> dict =
      storage->GetDictionary(GURL("https://origin1.test/testfile?hello"));
  EXPECT_FALSE(dict);
}

}  // namespace network
