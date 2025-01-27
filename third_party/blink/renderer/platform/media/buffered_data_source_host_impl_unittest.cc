// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class BufferedDataSourceHostImplTest : public testing::Test {
 public:
  BufferedDataSourceHostImplTest()
      : host_(base::BindRepeating(
                  &BufferedDataSourceHostImplTest::ProgressCallback,
                  base::Unretained(this)),
              &clock_) {}
  BufferedDataSourceHostImplTest(const BufferedDataSourceHostImplTest&) =
      delete;
  BufferedDataSourceHostImplTest& operator=(
      const BufferedDataSourceHostImplTest&) = delete;

  void Add() { host_.AddBufferedTimeRanges(&ranges_, base::Seconds(10)); }

  void ProgressCallback() { progress_callback_calls_++; }

 protected:
  int progress_callback_calls_ = 0;
  BufferedDataSourceHostImpl host_;
  media::Ranges<base::TimeDelta> ranges_;
  base::SimpleTestTickClock clock_;
};

TEST_F(BufferedDataSourceHostImplTest, Empty) {
  EXPECT_FALSE(host_.DidLoadingProgress());
  Add();
  EXPECT_EQ(0u, ranges_.size());
}

TEST_F(BufferedDataSourceHostImplTest, AddBufferedTimeRanges) {
  host_.AddBufferedByteRange(10, 20);
  host_.SetTotalBytes(100);
  Add();
  EXPECT_EQ(1u, ranges_.size());
  EXPECT_EQ(base::Seconds(1), ranges_.start(0));
  EXPECT_EQ(base::Seconds(2), ranges_.end(0));
}

TEST_F(BufferedDataSourceHostImplTest, AddBufferedTimeRanges_Merges) {
  ranges_.Add(base::Seconds(0), base::Seconds(1));
  host_.AddBufferedByteRange(10, 20);
  host_.SetTotalBytes(100);
  Add();
  EXPECT_EQ(1u, ranges_.size());
  EXPECT_EQ(base::Seconds(0), ranges_.start(0));
  EXPECT_EQ(base::Seconds(2), ranges_.end(0));
}

TEST_F(BufferedDataSourceHostImplTest, AddBufferedTimeRanges_Snaps) {
  host_.AddBufferedByteRange(5, 995);
  host_.SetTotalBytes(1000);
  Add();
  EXPECT_EQ(1u, ranges_.size());
  EXPECT_EQ(base::Seconds(0), ranges_.start(0));
  EXPECT_EQ(base::Seconds(10), ranges_.end(0));
}

TEST_F(BufferedDataSourceHostImplTest, SetTotalBytes) {
  host_.AddBufferedByteRange(10, 20);
  Add();
  EXPECT_EQ(0u, ranges_.size());

  host_.SetTotalBytes(100);
  Add();
  EXPECT_EQ(1u, ranges_.size());
}

TEST_F(BufferedDataSourceHostImplTest, DidLoadingProgress) {
  host_.AddBufferedByteRange(10, 20);
  EXPECT_TRUE(host_.DidLoadingProgress());
  EXPECT_FALSE(host_.DidLoadingProgress());
}

TEST_F(BufferedDataSourceHostImplTest, CanPlayThrough) {
  host_.SetTotalBytes(100000);
  EXPECT_EQ(100000,
            host_.UnloadedBytesInInterval(Interval<int64_t>(0, 100000)));
  host_.AddBufferedByteRange(0, 10000);
  clock_.Advance(base::Seconds(1));
  host_.AddBufferedByteRange(10000, 20000);
  clock_.Advance(base::Seconds(1));
  host_.AddBufferedByteRange(20000, 30000);
  clock_.Advance(base::Seconds(1));
  host_.AddBufferedByteRange(30000, 40000);
  clock_.Advance(base::Seconds(1));
  host_.AddBufferedByteRange(40000, 50000);
  clock_.Advance(base::Seconds(1));
  EXPECT_EQ(50000, host_.UnloadedBytesInInterval(Interval<int64_t>(0, 100000)));
  host_.AddBufferedByteRange(50000, 60000);
  clock_.Advance(base::Seconds(1));
  host_.AddBufferedByteRange(60000, 70000);
  clock_.Advance(base::Seconds(1));
  host_.AddBufferedByteRange(70000, 80000);
  clock_.Advance(base::Seconds(1));
  host_.AddBufferedByteRange(80000, 90000);
  // Download rate is allowed to be estimated low, but not high.
  EXPECT_LE(host_.DownloadRate(), 10000.0f);
  EXPECT_GE(host_.DownloadRate(), 9000.0f);
  EXPECT_EQ(10000, host_.UnloadedBytesInInterval(Interval<int64_t>(0, 100000)));
  EXPECT_EQ(9, progress_callback_calls_);
  // If the video is 0.1s we can't play through.
  EXPECT_FALSE(
      host_.CanPlayThrough(base::TimeDelta(), base::Seconds(0.01), 1.0));
  // If the video is 1000s we can play through.
  EXPECT_TRUE(
      host_.CanPlayThrough(base::TimeDelta(), base::Seconds(1000.0), 1.0));
  // No more downloads for 1000 seconds...
  clock_.Advance(base::Seconds(1000));
  // Can't play through..
  EXPECT_FALSE(
      host_.CanPlayThrough(base::TimeDelta(), base::Seconds(100.0), 1.0));
  host_.AddBufferedByteRange(90000, 100000);
  clock_.Advance(base::Seconds(1));
  EXPECT_EQ(0, host_.UnloadedBytesInInterval(Interval<int64_t>(0, 100000)));

  // Media is fully downloaded, so we can certainly play through, even if
  // we only have 0.01 seconds to do it.
  EXPECT_TRUE(
      host_.CanPlayThrough(base::TimeDelta(), base::Seconds(0.01), 1.0));
}

TEST_F(BufferedDataSourceHostImplTest, CanPlayThroughSmallAdvances) {
  host_.SetTotalBytes(20000);
  EXPECT_EQ(20000, host_.UnloadedBytesInInterval(Interval<int64_t>(0, 20000)));
  for (int j = 1; j <= 100; j++) {
    host_.AddBufferedByteRange(0, j * 100);
    clock_.Advance(base::Seconds(0.01));
  }
  // Download rate is allowed to be estimated low, but not high.
  EXPECT_LE(host_.DownloadRate(), 10000.0f);
  EXPECT_GE(host_.DownloadRate(), 9000.0f);
  EXPECT_EQ(10000, host_.UnloadedBytesInInterval(Interval<int64_t>(0, 20000)));
  EXPECT_EQ(100, progress_callback_calls_);
  // If the video is 0.1s we can't play through.
  EXPECT_FALSE(
      host_.CanPlayThrough(base::TimeDelta(), base::Seconds(0.01), 1.0));
  // If the video is 1000s we can play through.
  EXPECT_TRUE(
      host_.CanPlayThrough(base::TimeDelta(), base::Seconds(1000.0), 1.0));
}

}  // namespace blink
