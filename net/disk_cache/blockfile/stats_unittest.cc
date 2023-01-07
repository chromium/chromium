// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/stats.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

TEST(DiskCacheStatsTest, Init) {
  disk_cache::Stats stats;
  EXPECT_TRUE(stats.Init(nullptr, 0, disk_cache::Addr()));
  EXPECT_EQ(0, stats.GetCounter(disk_cache::Stats::TRIM_ENTRY));
}

TEST(DiskCacheStatsTest, InitWithEmptyBuffer) {
  disk_cache::Stats stats;
  int required_len = stats.StorageSize();
  auto storage = std::make_unique<char[]>(required_len);
  memset(storage.get(), 0, required_len);

  ASSERT_TRUE(stats.Init(storage.get(), required_len, disk_cache::Addr()));
  EXPECT_EQ(0, stats.GetCounter(disk_cache::Stats::TRIM_ENTRY));
}

TEST(DiskCacheStatsTest, FailsInit) {
  disk_cache::Stats stats;
  int required_len = stats.StorageSize();
  auto storage = std::make_unique<char[]>(required_len);
  memset(storage.get(), 0, required_len);

  // Try a small buffer.
  EXPECT_LT(200, required_len);
  disk_cache::Addr addr;
  EXPECT_FALSE(stats.Init(storage.get(), 200, addr));

  // Try a buffer with garbage.
  memset(storage.get(), 'a', required_len);
  EXPECT_FALSE(stats.Init(storage.get(), required_len, addr));
}

TEST(DiskCacheStatsTest, SaveRestore) {
  auto stats = std::make_unique<disk_cache::Stats>();

  disk_cache::Addr addr(5);
  ASSERT_TRUE(stats->Init(nullptr, 0, addr));
  stats->SetCounter(disk_cache::Stats::CREATE_ERROR, 11);
  stats->SetCounter(disk_cache::Stats::DOOM_ENTRY, 13);
  stats->OnEvent(disk_cache::Stats::MIN_COUNTER);
  stats->OnEvent(disk_cache::Stats::TRIM_ENTRY);
  stats->OnEvent(disk_cache::Stats::DOOM_RECENT);

  int required_len = stats->StorageSize();
  auto storage = std::make_unique<char[]>(required_len);
  disk_cache::Addr out_addr;
  int real_len = stats->SerializeStats(storage.get(), required_len, &out_addr);
  EXPECT_GE(required_len, real_len);
  EXPECT_EQ(out_addr, addr);

  stats = std::make_unique<disk_cache::Stats>();
  ASSERT_TRUE(stats->Init(storage.get(), real_len, addr));
  EXPECT_EQ(1, stats->GetCounter(disk_cache::Stats::MIN_COUNTER));
  EXPECT_EQ(1, stats->GetCounter(disk_cache::Stats::TRIM_ENTRY));
  EXPECT_EQ(1, stats->GetCounter(disk_cache::Stats::DOOM_RECENT));
  EXPECT_EQ(0, stats->GetCounter(disk_cache::Stats::OPEN_HIT));
  EXPECT_EQ(0, stats->GetCounter(disk_cache::Stats::READ_DATA));
  EXPECT_EQ(0, stats->GetCounter(disk_cache::Stats::LAST_REPORT_TIMER));
  EXPECT_EQ(11, stats->GetCounter(disk_cache::Stats::CREATE_ERROR));
  EXPECT_EQ(13, stats->GetCounter(disk_cache::Stats::DOOM_ENTRY));

  // Now pass the whole buffer. It shoulod not matter that there is unused
  // space at the end.
  stats = std::make_unique<disk_cache::Stats>();
  ASSERT_TRUE(stats->Init(storage.get(), required_len, addr));
  EXPECT_EQ(1, stats->GetCounter(disk_cache::Stats::MIN_COUNTER));
  EXPECT_EQ(1, stats->GetCounter(disk_cache::Stats::TRIM_ENTRY));
  EXPECT_EQ(1, stats->GetCounter(disk_cache::Stats::DOOM_RECENT));
  EXPECT_EQ(0, stats->GetCounter(disk_cache::Stats::OPEN_HIT));
  EXPECT_EQ(0, stats->GetCounter(disk_cache::Stats::READ_DATA));
  EXPECT_EQ(0, stats->GetCounter(disk_cache::Stats::LAST_REPORT_TIMER));
  EXPECT_EQ(11, stats->GetCounter(disk_cache::Stats::CREATE_ERROR));
  EXPECT_EQ(13, stats->GetCounter(disk_cache::Stats::DOOM_ENTRY));
}
