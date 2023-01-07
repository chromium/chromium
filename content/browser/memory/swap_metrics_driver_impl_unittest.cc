// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_driver_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// A SwapMetricsDriver that mocks what a platform-dependent driver does, but
// with control over when it fails so we can test error conditions.
class MockSwapMetricsDriver : public SwapMetricsDriverImpl {
 public:
  MockSwapMetricsDriver(std::unique_ptr<Delegate> delegate,
                        const base::TimeDelta update_interval,
                        const bool fail_on_update)
      : SwapMetricsDriverImpl(std::move(delegate), update_interval),
        fail_on_update_(fail_on_update) {}

  ~MockSwapMetricsDriver() override = default;

 protected:
  SwapMetricsDriver::SwapMetricsUpdateResult UpdateMetricsInternal(
      base::TimeDelta interval) override {
    if (fail_on_update_)
      return SwapMetricsDriver::SwapMetricsUpdateResult::
          kSwapMetricsUpdateFailed;

    if (interval.is_zero())
      return SwapMetricsDriver::SwapMetricsUpdateResult::
          kSwapMetricsUpdateSuccess;

    delegate_->OnSwapInCount(0, interval);
    delegate_->OnSwapOutCount(0, interval);
    delegate_->OnDecompressedPageCount(0, interval);
    delegate_->OnCompressedPageCount(0, interval);

    return SwapMetricsDriver::SwapMetricsUpdateResult::
        kSwapMetricsUpdateSuccess;
  }

 private:
  bool fail_on_update_;
};

// A SwapMetricsDriver::Delegate that counts the number of updates for each
// metric.
class SwapMetricsDelegateCounter : public SwapMetricsDriver::Delegate {
 public:
  SwapMetricsDelegateCounter()
      : num_swap_in_updates_(0),
        num_swap_out_updates_(0),
        num_decompressed_updates_(0),
        num_compressed_updates_(0),
        num_updates_failed_(0) {}

  SwapMetricsDelegateCounter(const SwapMetricsDelegateCounter&) = delete;
  SwapMetricsDelegateCounter& operator=(const SwapMetricsDelegateCounter&) =
      delete;

  ~SwapMetricsDelegateCounter() override = default;

  void OnSwapInCount(uint64_t count, base::TimeDelta interval) override {
    ++num_swap_in_updates_;
  }

  void OnSwapOutCount(uint64_t count, base::TimeDelta interval) override {
    ++num_swap_out_updates_;
  }

  void OnDecompressedPageCount(uint64_t count,
                               base::TimeDelta interval) override {
    ++num_decompressed_updates_;
  }

  void OnCompressedPageCount(uint64_t count,
                             base::TimeDelta interval) override {
    ++num_compressed_updates_;
  }

  void OnUpdateMetricsFailed() override { ++num_updates_failed_; }

  size_t num_swap_in_updates() const { return num_swap_in_updates_; }
  size_t num_swap_out_updates() const { return num_swap_out_updates_; }
  size_t num_decompressed_updates() const { return num_decompressed_updates_; }
  size_t num_compressed_updates() const { return num_compressed_updates_; }
  size_t num_updates_failed() const { return num_updates_failed_; }

 private:
  size_t num_swap_in_updates_;
  size_t num_swap_out_updates_;
  size_t num_decompressed_updates_;
  size_t num_compressed_updates_;
  size_t num_updates_failed_;
};

// The time delta between updates must non-zero for the delegate callbacks to be
// invoked.
constexpr base::TimeDelta kUpdateDelay = base::Milliseconds(50);

}  // namespace

class TestSwapMetricsDriver : public testing::Test {
 public:
  static std::unique_ptr<SwapMetricsDriver> CreateDriver(
      const base::TimeDelta update_interval,
      bool fail_on_update) {
    SwapMetricsDelegateCounter* delegate = new SwapMetricsDelegateCounter();
    return base::WrapUnique<SwapMetricsDriver>(new MockSwapMetricsDriver(
        base::WrapUnique<SwapMetricsDriver::Delegate>(delegate),
        update_interval, fail_on_update));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TestSwapMetricsDriver, ExpectedMetricCounts) {
  std::unique_ptr<SwapMetricsDriver> driver =
      CreateDriver(base::TimeDelta(), false);

  auto result = driver->InitializeMetrics();
  EXPECT_EQ(
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess,
      result);

  base::PlatformThread::Sleep(kUpdateDelay);
  result = driver->UpdateMetrics();
  EXPECT_EQ(
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess,
      result);

  base::PlatformThread::Sleep(kUpdateDelay);
  result = driver->UpdateMetrics();
  EXPECT_EQ(
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess,
      result);

  auto* delegate = static_cast<SwapMetricsDelegateCounter*>(
      static_cast<SwapMetricsDriverImpl*>(driver.get())->delegate_.get());

  EXPECT_EQ(2UL, delegate->num_swap_in_updates());
  EXPECT_EQ(2UL, delegate->num_swap_out_updates());
  EXPECT_EQ(2UL, delegate->num_decompressed_updates());
  EXPECT_EQ(2UL, delegate->num_compressed_updates());
}

TEST_F(TestSwapMetricsDriver, TimerStartSuccess) {
  std::unique_ptr<SwapMetricsDriver> driver =
      CreateDriver(base::Seconds(60), false);
  auto result = driver->Start();
  EXPECT_EQ(
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateSuccess,
      result);
}

TEST_F(TestSwapMetricsDriver, TimerStartFail) {
  std::unique_ptr<SwapMetricsDriver> driver =
      CreateDriver(base::Seconds(60), true);
  auto result = driver->Start();
  EXPECT_EQ(
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateFailed,
      result);
}

TEST_F(TestSwapMetricsDriver, UpdateMetricsFail) {
  std::unique_ptr<SwapMetricsDriver> driver =
      CreateDriver(base::Seconds(60), true);
  auto result = driver->InitializeMetrics();
  EXPECT_EQ(
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateFailed,
      result);
  result = driver->InitializeMetrics();
  EXPECT_EQ(
      SwapMetricsDriver::SwapMetricsUpdateResult::kSwapMetricsUpdateFailed,
      result);
  auto* delegate = static_cast<SwapMetricsDelegateCounter*>(
      static_cast<SwapMetricsDriverImpl*>(driver.get())->delegate_.get());
  EXPECT_EQ(2UL, delegate->num_updates_failed());
}

}  // namespace content
