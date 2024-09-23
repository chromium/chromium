// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/simple_lru_cache.h"

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

constexpr auto kEmptyEntrySize = SimpleLruCache::kEmptyEntrySize;

class SimpleLruCacheTest : public testing::TestWithParam<bool> {
 public:
  SimpleLruCacheTest() {
    scoped_feature_list_.InitWithFeatureState(features::kInMemoryCodeCache,
                                              GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(SimpleLruCacheTest, Empty) {
  const std::string kKey = "hello";
  SimpleLruCache cache(/*capacity=*/100 * 1024);

  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Has(kKey));
}

TEST_P(SimpleLruCacheTest, PutAndGet) {
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");
  // Note that kKey{i} doesn't necessarily correspond to kData{i} or
  // kResponseTime{i}.
  const std::vector<uint8_t> kData1 = {0x08};
  const std::vector<uint8_t> kData2 = {0x02, 0x09};
  const std::vector<uint8_t> kData3 = {0x03, 0x0a, 0xf1};
  const std::vector<uint8_t> kData4 = {0x04, 0x02, 0x00, 0x30};
  const base::Time kResponseTime1 = base::Time::UnixEpoch() + base::Hours(1);
  const base::Time kResponseTime2 = base::Time::UnixEpoch() + base::Hours(2);
  const base::Time kResponseTime3 = base::Time::UnixEpoch() + base::Hours(3);
  const base::Time kResponseTime4 = base::Time::UnixEpoch() + base::Hours(4);

  SimpleLruCache cache(/*capacity=*/100 * 1024);

  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_FALSE(cache.Has(kKey3));
  EXPECT_FALSE(cache.Has(kKey4));

  cache.Put(kKey1, kResponseTime1, base::make_span(kData1));
  EXPECT_EQ(cache.GetSize(), 1 + 4 + kEmptyEntrySize);
  EXPECT_TRUE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_FALSE(cache.Has(kKey3));
  EXPECT_FALSE(cache.Has(kKey4));

  // Updates the entry.
  cache.Put(kKey1, kResponseTime2, base::make_span(kData2));
  EXPECT_EQ(cache.GetSize(), 2 + 4 + kEmptyEntrySize);
  EXPECT_TRUE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_FALSE(cache.Has(kKey3));
  EXPECT_FALSE(cache.Has(kKey4));

  cache.Put(kKey2, kResponseTime3, base::make_span(kData3));
  EXPECT_EQ(cache.GetSize(), 5 + 8 + 2 * kEmptyEntrySize);
  EXPECT_TRUE(cache.Has(kKey1));
  EXPECT_TRUE(cache.Has(kKey2));
  EXPECT_FALSE(cache.Has(kKey3));
  EXPECT_FALSE(cache.Has(kKey4));

  // We don't create an entry for `kKey3` intentionally.
  cache.Put(kKey4, kResponseTime4, base::make_span(kData4));
  EXPECT_EQ(cache.GetSize(), 9 + 12 + 3 * kEmptyEntrySize);
  const auto result1 = cache.Get(kKey1);
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(result1->response_time, kResponseTime2);
  const auto result2 = cache.Get(kKey2);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2->response_time, kResponseTime3);
  const auto result3 = cache.Get(kKey3);
  ASSERT_FALSE(result3.has_value());
  const auto result4 = cache.Get(kKey4);
  ASSERT_TRUE(result4.has_value());
  EXPECT_EQ(result4->response_time, kResponseTime4);

  if (base::FeatureList::IsEnabled(features::kInMemoryCodeCache)) {
    EXPECT_EQ(base::ToVector(result1->data), kData2);
    EXPECT_EQ(base::ToVector(result2->data), kData3);
    EXPECT_EQ(base::ToVector(result4->data), kData4);
  } else {
    EXPECT_EQ(result1->data.size(), 0u);
    EXPECT_EQ(result2->data.size(), 0u);
    EXPECT_EQ(result4->data.size(), 0u);
  }
}

TEST_P(SimpleLruCacheTest, PutAndEvict) {
  const std::string kKey("key1");
  const uint8_t kData1[2] = {0x08, 0x09};
  const uint8_t kData2[1] = {0x02};
  const uint8_t kData3[2] = {0x03, 0x0a};
  const base::Time kResponseTime1 = base::Time::UnixEpoch() + base::Hours(1);
  const base::Time kResponseTime2 = base::Time::UnixEpoch() + base::Hours(2);
  const base::Time kResponseTime3 = base::Time::UnixEpoch() + base::Hours(3);

  SimpleLruCache cache(/*capacity=*/kEmptyEntrySize + kKey.size() + 1);

  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Has(kKey));

  // This entry is immediately evicted because the size excceeds the capacity.
  cache.Put(kKey, kResponseTime1, base::make_span(kData1));
  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Has(kKey));

  // This entry stays.
  cache.Put(kKey, kResponseTime2, base::make_span(kData2));
  EXPECT_EQ(cache.GetSize(), 1 + kKey.size() + kEmptyEntrySize);
  EXPECT_TRUE(cache.Has(kKey));

  // An updated entry can also be evicted.
  cache.Put(kKey, kResponseTime3, base::make_span(kData3));
  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Has(kKey));
}

TEST_P(SimpleLruCacheTest, LRU) {
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");
  const base::Time kResponseTime = base::Time::UnixEpoch();

  SimpleLruCache cache(kEmptyEntrySize * 2 + 8);

  cache.Put(kKey1, kResponseTime, base::span<uint8_t>());
  cache.Put(kKey2, kResponseTime, base::span<uint8_t>());
  cache.Put(kKey3, kResponseTime, base::span<uint8_t>());
  cache.Put(kKey4, kResponseTime, base::span<uint8_t>());

  // The last two entries are kept.
  EXPECT_EQ(cache.GetSize(), 2 * kEmptyEntrySize + 8);
  EXPECT_FALSE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_TRUE(cache.Has(kKey3));
  EXPECT_TRUE(cache.Has(kKey4));
}

TEST_P(SimpleLruCacheTest, LRUAndGet) {
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");
  const base::Time kResponseTime = base::Time::UnixEpoch();

  SimpleLruCache cache(kEmptyEntrySize * 2 + 8);

  cache.Put(kKey1, kResponseTime, base::span<uint8_t>());
  cache.Put(kKey2, kResponseTime, base::span<uint8_t>());
  // This call updates the access time.
  EXPECT_TRUE(cache.Has(kKey1));
  cache.Put(kKey3, kResponseTime, base::span<uint8_t>());

  EXPECT_EQ(cache.GetSize(), 2 * kEmptyEntrySize + 8);
  EXPECT_TRUE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_TRUE(cache.Has(kKey3));
  EXPECT_FALSE(cache.Has(kKey4));
}

TEST_P(SimpleLruCacheTest, Delete) {
  const base::Time kResponseTime = base::Time::UnixEpoch();
  const std::string kKey1("key1");
  const std::string kKey2("key2");
  const std::string kKey3("key3");
  const std::string kKey4("key4");
  const uint8_t kData1[1] = {0x08};
  const uint8_t kData2[2] = {0x02, 0x09};
  const uint8_t kData3[3] = {0x03, 0x0a, 0xf1};
  const uint8_t kData4[4] = {0x04, 0x02, 0x00, 0x30};

  SimpleLruCache cache(/*capacity=*/1024 * 1024);

  cache.Put(kKey1, kResponseTime, base::make_span(kData1));
  cache.Put(kKey2, kResponseTime, base::make_span(kData2));
  cache.Put(kKey3, kResponseTime, base::make_span(kData3));
  cache.Put(kKey4, kResponseTime, base::make_span(kData4));

  EXPECT_EQ(cache.GetSize(), 4 * kEmptyEntrySize + 16 + 10);
  EXPECT_TRUE(cache.Has(kKey1));
  EXPECT_TRUE(cache.Has(kKey2));
  EXPECT_TRUE(cache.Has(kKey3));
  EXPECT_TRUE(cache.Has(kKey4));

  cache.Delete(kKey2);
  EXPECT_EQ(cache.GetSize(), 3 * kEmptyEntrySize + 12 + 8);
  EXPECT_TRUE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_TRUE(cache.Has(kKey3));
  EXPECT_TRUE(cache.Has(kKey4));

  cache.Delete(kKey2);
  EXPECT_EQ(cache.GetSize(), 3 * kEmptyEntrySize + 12 + 8);
  EXPECT_TRUE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_TRUE(cache.Has(kKey3));
  EXPECT_TRUE(cache.Has(kKey4));

  cache.Delete(kKey1);
  cache.Delete(kKey2);
  cache.Delete(kKey3);
  cache.Delete(kKey4);
  EXPECT_EQ(cache.GetSize(), 0u);
  EXPECT_FALSE(cache.Has(kKey1));
  EXPECT_FALSE(cache.Has(kKey2));
  EXPECT_FALSE(cache.Has(kKey3));
  EXPECT_FALSE(cache.Has(kKey4));
}

TEST_P(SimpleLruCacheTest, Clear) {
  const base::Time kResponseTime = base::Time::UnixEpoch();
  const std::string kKey("key1");
  const uint8_t kData[1] = {0x08};

  SimpleLruCache cache(/*capacity=*/1024 * 1024);

  cache.Put(kKey, kResponseTime, base::make_span(kData));

  EXPECT_TRUE(cache.Has(kKey));
  EXPECT_GT(cache.GetSize(), 0u);

  cache.Clear();

  EXPECT_FALSE(cache.Has(kKey));
  EXPECT_EQ(cache.GetSize(), 0u);
}

INSTANTIATE_TEST_SUITE_P(SimpleLruCacheTest,
                         SimpleLruCacheTest,
                         testing::Bool());

}  // namespace
}  // namespace content
