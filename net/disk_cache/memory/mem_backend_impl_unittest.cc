// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/memory/mem_backend_impl.h"

#include <memory>
#include <string>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

namespace {

constexpr int kEntryDataSize = 1024 * 1024;  // 1 MiB
constexpr int kNumEntries = 10;
// Add some padding to account for key sizes and internal overhead.
constexpr int kTestCacheSize = kEntryDataSize * kNumEntries + 4096;

class MemBackendImplTest : public testing::Test {
 public:
  MemBackendImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        backend_(MemBackendImpl::CreateBackend(kTestCacheSize, nullptr)) {
    CHECK(backend_);
  }

 protected:
  void SimulatePressureNotification(base::MemoryPressureLevel level) {
    base::MemoryPressureListener::SimulatePressureNotificationAsync(
        level, task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  void CreateAndWriteEntry(const std::string& key) {
    TestEntryResultCompletionCallback cb;
    EntryResult result =
        cb.GetResult(backend_->CreateEntry(key, net::HIGHEST, cb.callback()));
    ASSERT_EQ(result.net_error(), net::OK);
    Entry* entry = result.ReleaseEntry();

    auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kEntryDataSize);
    std::ranges::fill(buf->span(), 'a');
    net::TestCompletionCallback cb2;
    int rv = cb2.GetResult(entry->WriteData(0, 0, buf.get(), kEntryDataSize,
                                            cb2.callback(), true));
    ASSERT_EQ(rv, kEntryDataSize);
    entry->Close();
  }

  bool EntryExists(const std::string& key) {
    TestEntryResultCompletionCallback cb;
    EntryResult result =
        cb.GetResult(backend_->OpenEntry(key, net::HIGHEST, cb.callback()));
    if (result.net_error() == net::OK) {
      result.ReleaseEntry()->Close();
      return true;
    }
    return false;
  }

  base::test::TaskEnvironment task_environment_;
  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  std::unique_ptr<MemBackendImpl> backend_;
};

TEST_F(MemBackendImplTest, EvictionMargin) {
  // Fill the cache to capacity (10 entries).
  for (int i = 0; i < kNumEntries; ++i) {
    CreateAndWriteEntry("key" + base::NumberToString(i));
  }

  // Adding one more entry should trigger eviction with a 10% margin.
  // 10% of 10 entries is 1 entry, so the target is 9 entries.
  // Since we have 11 entries (10 old + 1 new), 2 entries should be evicted.
  CreateAndWriteEntry("key10");

  EXPECT_FALSE(EntryExists("key0"));
  EXPECT_FALSE(EntryExists("key1"));
  for (int i = 2; i <= 10; ++i) {
    EXPECT_TRUE(EntryExists("key" + base::NumberToString(i)));
  }
}

TEST_F(MemBackendImplTest, MemoryPressure) {
  // 1. Fill the cache.
  for (int i = 0; i < kNumEntries; ++i) {
    CreateAndWriteEntry("key" + base::NumberToString(i));
  }

  // 2. Simulate MODERATE memory pressure (50% limit).
  // Memory pressure evicts exactly to the new limit (no 10% margin).
  SimulatePressureNotification(base::MEMORY_PRESSURE_LEVEL_MODERATE);

  // key0 to key4 should be evicted.
  for (int i = 0; i < 5; ++i) {
    EXPECT_FALSE(EntryExists("key" + base::NumberToString(i)));
  }
  // key5 to key9 should remain.
  for (int i = 5; i < kNumEntries; ++i) {
    EXPECT_TRUE(EntryExists("key" + base::NumberToString(i)));
  }

  // 3. Simulate CRITICAL memory pressure (10% limit).
  SimulatePressureNotification(base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Only the most recent entry (key9) should remain.
  for (int i = 0; i < 9; ++i) {
    EXPECT_FALSE(EntryExists("key" + base::NumberToString(i)));
  }
  EXPECT_TRUE(EntryExists("key9"));

  // 4. Simulate NONE memory pressure.
  SimulatePressureNotification(base::MEMORY_PRESSURE_LEVEL_NONE);

  // Should be able to add new entries.
  CreateAndWriteEntry("key10");
}

}  // namespace

}  // namespace disk_cache
