// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/code_cache/generated_code_cache.h"

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/network_isolation_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class GeneratedCodeCacheTest : public testing::TestWithParam<bool> {
 public:
  // This should be larger than |kSmallDataLimit| in generated_code_cache.cc.
  static const size_t kLargeSizeInBytes = 8192;
  // This should be larger than |kLargeDataLimit| in generated_code_cache.cc.
  // Additionally, this shouldn't exceed 1/8 of the maximum cache size below,
  // |kMaxSizeInBytes|.
  static const size_t kVeryLargeSizeInBytes = 128 * 1024;
  static const size_t kMaxSizeInBytes = 1024 * 1024;
  static_assert(kMaxSizeInBytes / kVeryLargeSizeInBytes > 0UL,
                "Cache will be too small to hold a very large item");
  static constexpr char kInitialUrl[] = "http://example.com/script.js";
  static constexpr char kInitialOrigin[] = "http://example.com";
  static constexpr char kInitialData[] = "InitialData";

  GeneratedCodeCacheTest() {
    scoped_feature_list_.InitWithFeatureState(features::kInMemoryCodeCache,
                                              GetParam());
  }

  void SetUp() override {
    ASSERT_TRUE(cache_dir_.CreateUniqueTempDir());
    cache_path_ = cache_dir_.GetPath();
  }

  void TearDown() override {
    disk_cache::FlushCacheThreadForTesting();
    backend_ = nullptr;
    generated_code_cache_.reset();
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(cache_dir_.Delete()) << cache_dir_.GetPath();
  }

  // This function initializes the cache and waits till the transaction is
  // finished. When this function returns, the backend is already initialized.
  void InitializeCache(GeneratedCodeCache::CodeCacheType cache_type,
                       const char* initial_url = kInitialUrl,
                       const char* initial_origin = kInitialOrigin) {
    // Create code cache
    generated_code_cache_ = std::make_unique<GeneratedCodeCache>(
        cache_path_, kMaxSizeInBytes, cache_type);

    GeneratedCodeCache::GetBackendCallback callback = base::BindOnce(
        &GeneratedCodeCacheTest::GetBackendCallback, base::Unretained(this));
    generated_code_cache_->GetBackend(std::move(callback));

    WriteToCache(GURL(initial_url), GURL(initial_origin), kInitialData,
                 base::Time::Now());
    task_environment_.RunUntilIdle();
  }

  // This function initializes the cache and reopens it. When this function
  // returns, the backend initialization is not complete yet. This is used
  // to test the pending operaions path.
  void InitializeCacheAndReOpen(GeneratedCodeCache::CodeCacheType cache_type) {
    InitializeCache(cache_type);
    backend_ = nullptr;
    generated_code_cache_ = std::make_unique<GeneratedCodeCache>(
        cache_path_, kMaxSizeInBytes, cache_type);
  }

  void WriteToCache(const GURL& url,
                    const GURL& origin_lock,
                    const std::string& data,
                    base::Time response_time) {
    std::vector<uint8_t> vector_data(data.begin(), data.end());
    generated_code_cache_->WriteEntry(url, origin_lock,
                                      net::NetworkIsolationKey(), response_time,
                                      vector_data);
  }

  void DeleteFromCache(const GURL& url, const GURL& origin_lock) {
    generated_code_cache_->DeleteEntry(url, origin_lock,
                                       net::NetworkIsolationKey());
  }

  void FetchFromCache(const GURL& url, const GURL& origin_lock) {
    received_ = false;
    GeneratedCodeCache::ReadDataCallback callback = base::BindOnce(
        &GeneratedCodeCacheTest::FetchEntryCallback, base::Unretained(this));
    generated_code_cache_->FetchEntry(
        url, origin_lock, net::NetworkIsolationKey(), std::move(callback));
  }

  void DoomAll() {
    net::CompletionOnceCallback callback = base::BindOnce(
        &GeneratedCodeCacheTest::DoomAllCallback, base::Unretained(this));
    backend_->DoomAllEntries(std::move(callback));
  }

  void GetBackendCallback(disk_cache::Backend* backend) { backend_ = backend; }

  void DoomAllCallback(int rv) {}

  void FetchEntryCallback(const base::Time& response_time,
                          mojo_base::BigBuffer data) {
    if (data.size() == 0) {
      received_ = true;
      received_null_ = true;
      received_response_time_ = response_time;
      return;
    }
    std::string str(data.data(), data.data() + data.size());
    received_ = true;
    received_null_ = false;
    received_data_ = str;
    received_response_time_ = response_time;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir cache_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<GeneratedCodeCache> generated_code_cache_;
  std::string received_data_;
  base::Time received_response_time_;
  bool received_;
  bool received_null_;
  base::FilePath cache_path_;
  raw_ptr<disk_cache::Backend> backend_ = nullptr;
};

constexpr char GeneratedCodeCacheTest::kInitialUrl[];
constexpr char GeneratedCodeCacheTest::kInitialOrigin[];
constexpr char GeneratedCodeCacheTest::kInitialData[];
const size_t GeneratedCodeCacheTest::kMaxSizeInBytes;

TEST_P(GeneratedCodeCacheTest, GetResourceURLFromKey) {
  // These must be kept in sync with the values in generated_code_cache.cc.
  constexpr char kPrefix[] = "_key";
  constexpr char kSeparator[] = " \n";

  // Test that we correctly extract the resource URL from a key.
  std::string key(kPrefix);
  key.append(kInitialUrl);
  key.append(kSeparator);
  key.append(kInitialOrigin);

  EXPECT_EQ(GeneratedCodeCache::GetResourceURLFromKey(key), kInitialUrl);

  // Invalid key formats should return the empty string.
  ASSERT_TRUE(GeneratedCodeCache::GetResourceURLFromKey("").empty());
  ASSERT_TRUE(GeneratedCodeCache::GetResourceURLFromKey("foobar").empty());
  ASSERT_TRUE(
      GeneratedCodeCache::GetResourceURLFromKey(
          "43343250B630900F20597168708E14F17A6263F5251FCA10746EA7BDEA881085")
          .empty());
}

TEST_P(GeneratedCodeCacheTest, CheckResponseTime) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForScript";
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, FetchEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(kInitialData, received_data_);
}

TEST_P(GeneratedCodeCacheTest, WriteEntry) {
  GURL new_url("http://example1.com/script.js");
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForScript";
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin_lock, data, response_time);
  FetchFromCache(new_url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, WriteLargeEntry) {
  GURL new_url("http://example1.com/script.js");
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin_lock, large_data, response_time);
  FetchFromCache(new_url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(large_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, WriteVeryLargeEntry) {
  GURL new_url("http://example1.com/script.js");
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kVeryLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin_lock, large_data, response_time);
  FetchFromCache(new_url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(large_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, DeleteEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  DeleteFromCache(url, origin_lock);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_P(GeneratedCodeCacheTest, WriteEntryWithEmptyData) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, std::string(), response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, WriteEntryFailure) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  base::Time response_time = base::Time::Now();
  std::string too_big_data(kMaxSizeInBytes * 8, 0);
  WriteToCache(url, origin_lock, too_big_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  // Fetch should return empty data, with invalid response time.
  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
  EXPECT_EQ(base::Time(), received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, WriteEntryFailureOutOfOrder) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  // Dooming adds pending activity for all entries. This makes the following
  // write block for stream 0, while the stream 1 write fails synchronously. The
  // two callbacks are received in reverse order.
  DoomAll();
  base::Time response_time = base::Time::Now();
  std::string too_big_data(kMaxSizeInBytes * 8, 0);
  WriteToCache(url, origin_lock, too_big_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  // Fetch should return empty data, with invalid response time.
  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
  EXPECT_EQ(base::Time(), received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, FetchEntryPendingOp) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCacheAndReOpen(GeneratedCodeCache::CodeCacheType::kJavaScript);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(kInitialData, received_data_);
}

TEST_P(GeneratedCodeCacheTest, WriteEntryPendingOp) {
  GURL new_url("http://example1.com/script1.js");
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForScript";
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin_lock, data, response_time);
  FetchFromCache(new_url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, WriteLargeEntryPendingOp) {
  GURL new_url("http://example1.com/script1.js");
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin_lock, large_data, response_time);
  FetchFromCache(new_url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(large_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, WriteVeryLargeEntryPendingOp) {
  GURL new_url("http://example1.com/script1.js");
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kVeryLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(new_url, origin_lock, large_data, response_time);
  FetchFromCache(new_url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(large_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, DeleteEntryPendingOp) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCacheAndReOpen(GeneratedCodeCache::CodeCacheType::kJavaScript);
  DeleteFromCache(url, origin_lock);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_P(GeneratedCodeCacheTest, UpdateDataOfExistingEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string new_data = "SerializedCodeForScriptOverwrite";
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, new_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(new_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, UpdateDataOfSmallExistingEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string new_data(kLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, new_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(new_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, UpdateDataOfLargeExistingEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, large_data, response_time);
  std::string new_data = large_data + "Overwrite";
  response_time = base::Time::Now();
  WriteToCache(url, origin_lock, new_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(new_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, UpdateDataOfVeryLargeExistingEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kVeryLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, large_data, response_time);
  std::string new_data = large_data + "Overwrite";
  response_time = base::Time::Now();
  WriteToCache(url, origin_lock, new_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(new_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, TruncateDataOfLargeExistingEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, large_data, response_time);
  std::string new_data = "SerializedCodeForScriptOverwrite";
  response_time = base::Time::Now();
  WriteToCache(url, origin_lock, new_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(new_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, TruncateDataOfVeryLargeExistingEntry) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string large_data(kVeryLargeSizeInBytes, 'x');
  base::Time response_time = base::Time::Now();
  WriteToCache(url, origin_lock, large_data, response_time);
  std::string new_data = "SerializedCodeForScriptOverwrite";
  response_time = base::Time::Now();
  WriteToCache(url, origin_lock, new_data, response_time);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(new_data, received_data_);
  EXPECT_EQ(response_time, received_response_time_);
}

TEST_P(GeneratedCodeCacheTest, FetchFailsForNonexistingOrigin) {
  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  GURL new_origin_lock = GURL("http://not-example.com");
  FetchFromCache(GURL(kInitialUrl), new_origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  ASSERT_TRUE(received_null_);
}

TEST_P(GeneratedCodeCacheTest, FetchEntriesFromSameOrigin) {
  GURL url("http://example.com/script.js");
  GURL second_url("http://script.com/one.js");
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data_first_resource = "SerializedCodeForFirstResource";
  WriteToCache(url, origin_lock, data_first_resource, base::Time());

  std::string data_second_resource = "SerializedCodeForSecondResource";
  WriteToCache(second_url, origin_lock, data_second_resource, base::Time());

  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_first_resource, received_data_);

  FetchFromCache(second_url, origin_lock);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_second_resource, received_data_);
}

TEST_P(GeneratedCodeCacheTest, FetchSucceedsFromDifferentOrigins) {
  GURL url("http://example.com/script.js");
  GURL origin_lock = GURL("http://example.com");
  GURL origin_lock1 = GURL("http://example1.com");

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data_origin = "SerializedCodeForFirstOrigin";
  WriteToCache(url, origin_lock, data_origin, base::Time());

  std::string data_origin1 = "SerializedCodeForSecondOrigin";
  WriteToCache(url, origin_lock1, data_origin1, base::Time());

  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_origin, received_data_);

  FetchFromCache(url, origin_lock1);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data_origin1, received_data_);
}

TEST_P(GeneratedCodeCacheTest, VeryLargeEntriesAreMerged) {
  GURL url("http://example.com/script.js");
  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);

  // Write more copies of the same resource than the cache can hold unless they
  // are merged by content.
  for (size_t i = 0; i < 2 * kMaxSizeInBytes / kVeryLargeSizeInBytes; ++i) {
    GURL origin_lock = GURL(std::string("http://example") +
                            base::NumberToString(i) + std::string(".com"));
    std::string large_data(kVeryLargeSizeInBytes, 'x');
    WriteToCache(url, origin_lock, large_data, base::Time());
  }

  for (size_t i = 0; i < 2 * kMaxSizeInBytes / kVeryLargeSizeInBytes; ++i) {
    GURL origin_lock = GURL(std::string("http://example") +
                            base::NumberToString(i) + std::string(".com"));
    std::string large_data(kVeryLargeSizeInBytes, 'x');
    FetchFromCache(url, origin_lock);
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(received_);
    EXPECT_EQ(large_data, received_data_);
    received_ = false;
    received_data_ = std::string();
  }
}

TEST_P(GeneratedCodeCacheTest, StressVeryLargeEntries) {
  GURL url("http://example.com/script.js");
  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  // Fill the cache with very large data keyed by the SHA-256 checksum.
  char data1 = 0;
  for (size_t i = 0; i < kMaxSizeInBytes / kVeryLargeSizeInBytes - 1;
       ++i, ++data1) {
    GURL origin_lock = GURL(std::string("http://example") +
                            base::NumberToString(i) + std::string(".com"));
    std::string large_data(kVeryLargeSizeInBytes, data1);
    WriteToCache(url, origin_lock, large_data, base::Time());
  }

  // Fill the cache with new data. The old entries should be purged to make
  // room for the new ones.
  char data2 = -128;
  for (size_t i = 0; i < kMaxSizeInBytes / kVeryLargeSizeInBytes - 1;
       ++i, ++data2) {
    GURL origin_lock = GURL(std::string("http://example") +
                            base::NumberToString(i) + std::string(".com"));
    std::string large_data(kVeryLargeSizeInBytes, data2);
    WriteToCache(url, origin_lock, large_data, base::Time());
  }

  data2 = -128;
  for (size_t i = 0; i < kMaxSizeInBytes / kVeryLargeSizeInBytes - 1;
       ++i, ++data2) {
    GURL origin_lock = GURL(std::string("http://example") +
                            base::NumberToString(i) + std::string(".com"));
    FetchFromCache(url, origin_lock);
    task_environment_.RunUntilIdle();
    // We can't depend too strongly on the disk cache storage heuristic. Verify
    // that if we received data, it's what we wrote.
    if (!received_null_) {
      std::string large_data(kVeryLargeSizeInBytes, data2);
      EXPECT_EQ(large_data, received_data_);
      received_ = false;
      received_data_ = std::string();
    }
  }
}

TEST_P(GeneratedCodeCacheTest, FetchSucceedsEmptyOriginLock) {
  GURL url("http://example.com/script.js");
  GURL origin_lock = GURL("");

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string data = "SerializedCodeForEmptyOrigin";
  WriteToCache(url, origin_lock, data, base::Time());

  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(data, received_data_);
}

TEST_P(GeneratedCodeCacheTest, FetchEmptyOriginVsValidOriginLocks) {
  GURL url("http://example.com/script.js");
  GURL empty_origin_lock = GURL("");
  GURL origin_lock = GURL("http://example.com");

  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript);
  std::string empty_origin_data = "SerializedCodeForEmptyOrigin";
  WriteToCache(url, empty_origin_lock, empty_origin_data, base::Time());

  std::string valid_origin_data = "SerializedCodeForValidOrigin";
  WriteToCache(url, origin_lock, valid_origin_data, base::Time());

  FetchFromCache(url, empty_origin_lock);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(empty_origin_data, received_data_);

  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_);
  EXPECT_EQ(valid_origin_data, received_data_);
}

TEST_P(GeneratedCodeCacheTest, WasmCache) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  InitializeCache(GeneratedCodeCache::CodeCacheType::kWebAssembly);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(received_);
  EXPECT_EQ(kInitialData, received_data_);
}

TEST_P(GeneratedCodeCacheTest, TestFailedBackendOpening) {
  GURL url(kInitialUrl);
  GURL origin_lock = GURL(kInitialOrigin);

  // Clear cache_path_ so the backend initialization fails.
  cache_path_.clear();
  InitializeCacheAndReOpen(GeneratedCodeCache::CodeCacheType::kJavaScript);
  FetchFromCache(url, origin_lock);
  task_environment_.RunUntilIdle();

  // We should still receive a callback.
  ASSERT_TRUE(received_);
  // We shouldn't receive any data.
  ASSERT_TRUE(received_null_);
}

// Tests the embedder specifying custom WebUI hostnames for metrics reporting.
class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  TestBrowserClient(const TestBrowserClient&) = delete;
  TestBrowserClient& operator=(const TestBrowserClient&) = delete;
  ~TestBrowserClient() override = default;

  // ContentBrowserClient:
  std::string GetWebUIHostnameForCodeCacheMetrics(
      const GURL& webui_url) const override {
    if (webui_url.host() == "foo") {
      return "Foo";
    }
    if (webui_url.host() == "bar") {
      return "Bar";
    }
    return std::string();
  }
};

class GeneratedCodeCacheMetricsTest : public GeneratedCodeCacheTest {
 public:
  GeneratedCodeCacheMetricsTest() {
    content::SetBrowserClientForTesting(&browser_client_);
  }
  ~GeneratedCodeCacheMetricsTest() override {
    content::SetBrowserClientForTesting(nullptr);
  }

 private:
  TestBrowserClient browser_client_;
};

TEST_P(GeneratedCodeCacheMetricsTest, JSWebUIMetricsWithEmbedderHostnames) {
  constexpr char kResource1Url[] = "chrome://foo/resource.js";
  constexpr char kResource2Url[] = "chrome://bar/resource.js";
  constexpr char kOriginUrl[] = "chrome://foo/";
  InitializeCache(GeneratedCodeCache::CodeCacheType::kWebUIJavaScript,
                  kResource1Url, kOriginUrl);
  base::HistogramTester histogram_tester;

  // Simulate a request for two resources against different data sources for a
  // single origin.
  generated_code_cache_->CollectStatisticsForTest(
      GURL(kResource1Url), GURL(kOriginUrl),
      content::GeneratedCodeCache::CacheEntryStatus::kUpdate);
  generated_code_cache_->CollectStatisticsForTest(
      GURL(kResource2Url), GURL(kOriginUrl),
      content::GeneratedCodeCache::CacheEntryStatus::kHit);

  // WebUI scripts report both the generic and WebUI specific metrics.
  histogram_tester.ExpectBucketCount(
      "SiteIsolatedCodeCache.JS.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kUpdate, 1);
  histogram_tester.ExpectBucketCount(
      "SiteIsolatedCodeCache.JS.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kHit, 1);
  histogram_tester.ExpectBucketCount(
      "SiteIsolatedCodeCache.JS.WebUI.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kUpdate, 1);
  histogram_tester.ExpectBucketCount(
      "SiteIsolatedCodeCache.JS.WebUI.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kHit, 1);

  // There should be two source specific resource hits.
  histogram_tester.ExpectUniqueSample(
      "SiteIsolatedCodeCache.JS.WebUI.Foo.Resource.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kUpdate, 1);
  histogram_tester.ExpectUniqueSample(
      "SiteIsolatedCodeCache.JS.WebUI.Bar.Resource.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kHit, 1);

  // Both resource requests should be reported against the same origin.
  histogram_tester.ExpectBucketCount(
      "SiteIsolatedCodeCache.JS.WebUI.Foo.Origin.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kUpdate, 1);
  histogram_tester.ExpectBucketCount(
      "SiteIsolatedCodeCache.JS.WebUI.Foo.Origin.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kHit, 1);
}

TEST_P(GeneratedCodeCacheMetricsTest, JavaScriptMetrics) {
  constexpr char kResourceUrl[] = "https://foo/resource.js";
  constexpr char kOriginUrl[] = "https://foo/";
  InitializeCache(GeneratedCodeCache::CodeCacheType::kJavaScript, kResourceUrl,
                  kOriginUrl);
  base::HistogramTester histogram_tester;

  generated_code_cache_->CollectStatisticsForTest(
      GURL(kResourceUrl), GURL(kOriginUrl),
      content::GeneratedCodeCache::CacheEntryStatus::kHit);
  histogram_tester.ExpectUniqueSample(
      "SiteIsolatedCodeCache.JS.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kHit, 1);
}

TEST_P(GeneratedCodeCacheMetricsTest, WebAssemblyMetrics) {
  constexpr char kResourceUrl[] = "https://foo/resource.wasm";
  constexpr char kOriginUrl[] = "https://foo/";
  InitializeCache(GeneratedCodeCache::CodeCacheType::kWebAssembly, kResourceUrl,
                  kOriginUrl);
  base::HistogramTester histogram_tester;

  generated_code_cache_->CollectStatisticsForTest(
      GURL(kResourceUrl), GURL(kOriginUrl),
      content::GeneratedCodeCache::CacheEntryStatus::kHit);
  histogram_tester.ExpectUniqueSample(
      "SiteIsolatedCodeCache.WASM.Behaviour",
      content::GeneratedCodeCache::CacheEntryStatus::kHit, 1);
}

INSTANTIATE_TEST_SUITE_P(GeneratedCodeCacheTest,
                         GeneratedCodeCacheTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(GeneratedCodeCacheMetricsTest,
                         GeneratedCodeCacheMetricsTest,
                         testing::Bool());

}  // namespace content
