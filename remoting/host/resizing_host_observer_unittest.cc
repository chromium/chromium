// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resizing_host_observer.h"

#include <list>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/screen_resolution.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

std::ostream& operator<<(std::ostream& os, const ScreenResolution& resolution) {
  return os << resolution.dimensions().width() << "x"
            << resolution.dimensions().height() << " @ "
            << resolution.dpi().x() << "x" << resolution.dpi().y();
}

bool operator==(const ScreenResolution& a, const ScreenResolution& b) {
  return a.Equals(b);
}

const int kDefaultDPI = 96;

ScreenResolution MakeResolution(int width, int height) {
  return ScreenResolution(webrtc::DesktopSize(width, height),
                          webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
}

class FakeDesktopResizer : public DesktopResizer {
 public:
  struct CallCounts {
    int set_resolution = 0;
    int restore_resolution = 0;
  };

  FakeDesktopResizer(bool exact_size_supported,
                     std::vector<ScreenResolution> supported_resolutions,
                     ScreenResolution* current_resolution,
                     CallCounts* call_counts,
                     bool check_final_resolution)
      : exact_size_supported_(exact_size_supported),
        initial_resolution_(*current_resolution),
        current_resolution_(current_resolution),
        supported_resolutions_(std::move(supported_resolutions)),
        call_counts_(call_counts),
        check_final_resolution_(check_final_resolution) {
  }

  ~FakeDesktopResizer() override {
    if (check_final_resolution_) {
      EXPECT_EQ(initial_resolution_, GetCurrentResolution());
    }
  }

  // remoting::DesktopResizer interface
  ScreenResolution GetCurrentResolution() override {
    return *current_resolution_;
  }
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) override {
    std::list<ScreenResolution> result(supported_resolutions_.begin(),
                                       supported_resolutions_.end());
    if (exact_size_supported_) {
      result.push_back(preferred);
    }
    return result;
  }
  void SetResolution(const ScreenResolution& resolution) override {
    *current_resolution_ = resolution;
    ++call_counts_->set_resolution;
  }
  void RestoreResolution(const ScreenResolution& resolution) override {
    *current_resolution_ = resolution;
    ++call_counts_->restore_resolution;
  }

 private:
  bool exact_size_supported_;
  ScreenResolution initial_resolution_;
  ScreenResolution *current_resolution_;
  std::vector<ScreenResolution> supported_resolutions_;
  CallCounts* call_counts_;
  bool check_final_resolution_;
};

class ResizingHostObserverTest : public testing::Test {
 public:
  ResizingHostObserverTest()
      : now_(base::TimeTicks::Now()) {
  }

  // This needs to be public because the derived test-case class needs to
  // pass it to Bind, which fails if it's protected.
  base::TimeTicks GetTime() {
    return now_;
  }

 protected:
  void InitDesktopResizer(const ScreenResolution& initial_resolution,
                         bool exact_size_supported,
                         std::vector<ScreenResolution> supported_resolutions,
                         bool restore_resolution) {
    current_resolution_ = initial_resolution;
    call_counts_ = FakeDesktopResizer::CallCounts();
    resizing_host_observer_ = std::make_unique<ResizingHostObserver>(
        std::make_unique<FakeDesktopResizer>(
            exact_size_supported, std::move(supported_resolutions),
            &current_resolution_, &call_counts_, restore_resolution),
        restore_resolution);
    resizing_host_observer_->SetNowFunctionForTesting(
        base::Bind(&ResizingHostObserverTest::GetTimeAndIncrement,
                   base::Unretained(this)));
  }

  ScreenResolution GetBestResolution(const ScreenResolution& client_size) {
    resizing_host_observer_->SetScreenResolution(client_size);
    return current_resolution_;
  }

  void VerifySizes(const std::vector<ScreenResolution>& client_sizes,
                   const std::vector<ScreenResolution>& expected_sizes) {
    ASSERT_EQ(client_sizes.size(), expected_sizes.size())
        << "Client and expected vectors must have the same length";
    for (auto client = client_sizes.begin(), expected = expected_sizes.begin();
         client != client_sizes.end() && expected != expected_sizes.end();
         ++client, ++expected) {
      ScreenResolution best_size = GetBestResolution(*client);
      EXPECT_EQ(*expected, best_size) << "Input resolution = " << *client;
    }
  }

  base::TimeTicks GetTimeAndIncrement() {
    base::TimeTicks result = now_;
    now_ += base::TimeDelta::FromSeconds(1);
    return result;
  }

  ScreenResolution current_resolution_;
  FakeDesktopResizer::CallCounts call_counts_;
  std::unique_ptr<ResizingHostObserver> resizing_host_observer_;
  base::TimeTicks now_;
};

// Check that the resolution isn't restored if it wasn't changed by this class.
TEST_F(ResizingHostObserverTest, NoRestoreResolution) {
  InitDesktopResizer(MakeResolution(640, 480), false,
                     std::vector<ScreenResolution>(), true);
  resizing_host_observer_.reset();
  EXPECT_EQ(0, call_counts_.restore_resolution);
}

// Check that the host is not resized if GetSupportedSizes returns an empty
// list (even if GetCurrentSize is supported).
TEST_F(ResizingHostObserverTest, EmptyGetSupportedSizes) {
  ScreenResolution initial = MakeResolution(640, 480);
  InitDesktopResizer(initial, false, std::vector<ScreenResolution>(), true);
  VerifySizes({MakeResolution(200, 100), MakeResolution(100, 200)},
              {initial, initial});
  resizing_host_observer_.reset();
  EXPECT_EQ(0, call_counts_.set_resolution);
  EXPECT_EQ(0, call_counts_.restore_resolution);
}

// Check that the restore flag is respected.
TEST_F(ResizingHostObserverTest, RestoreFlag) {
  ScreenResolution initial = MakeResolution(640, 480);
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(640, 480),
                                                   MakeResolution(1024, 768)};
  std::vector<ScreenResolution> client_sizes = {MakeResolution(1024, 768)};

  // Flag false
  InitDesktopResizer(initial, false, supported_sizes, false);
  VerifySizes(client_sizes, client_sizes);
  resizing_host_observer_.reset();
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(0, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(1024, 768), current_resolution_);

  // Flag true
  InitDesktopResizer(initial, false, supported_sizes, true);
  VerifySizes(client_sizes, client_sizes);
  resizing_host_observer_.reset();
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(1, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(640, 480), current_resolution_);
}

// Check that the size is restored if an empty ClientResolution is received.
TEST_F(ResizingHostObserverTest, RestoreOnEmptyClientResolution) {
  InitDesktopResizer(MakeResolution(640, 480), true,
                     std::vector<ScreenResolution>(), true);
  resizing_host_observer_->SetScreenResolution(MakeResolution(200, 100));
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(0, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(200, 100), current_resolution_);
  resizing_host_observer_->SetScreenResolution(MakeResolution(0, 0));
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(1, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(640, 480), current_resolution_);
}

// Check that if the implementation supports exact size matching, it is used.
TEST_F(ResizingHostObserverTest, SelectExactSize) {
  InitDesktopResizer(MakeResolution(640, 480), true,
                     std::vector<ScreenResolution>(), true);
  std::vector<ScreenResolution> client_sizes = {MakeResolution(200, 100),
                                                MakeResolution(100, 200),
                                                MakeResolution(640, 480),
                                                MakeResolution(480, 640),
                                                MakeResolution(1280, 1024)};
  VerifySizes(client_sizes, client_sizes);
  resizing_host_observer_.reset();
  EXPECT_EQ(1, call_counts_.restore_resolution);
}

// Check that if the implementation supports a size that is no larger than
// the requested size, then the largest such size is used.
TEST_F(ResizingHostObserverTest, SelectBestSmallerSize) {
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(639, 479),
                                                   MakeResolution(640, 480)};
  InitDesktopResizer(MakeResolution(640, 480), false, supported_sizes, true);
  VerifySizes({MakeResolution(639, 479),
               MakeResolution(640, 480),
               MakeResolution(641, 481),
               MakeResolution(999, 999)},
              {supported_sizes[0],
               supported_sizes[1],
               supported_sizes[1],
               supported_sizes[1]});
}

// Check that if the implementation supports only sizes that are larger than
// the requested size, then the one that requires the least down-scaling.
TEST_F(ResizingHostObserverTest, SelectBestScaleFactor) {
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(100, 100),
                                                   MakeResolution(200, 100)};
  InitDesktopResizer(MakeResolution(200, 100), false, supported_sizes, true);
  VerifySizes({MakeResolution(1, 1),
               MakeResolution(99, 99),
               MakeResolution(199, 99)},
              {supported_sizes[0],
               supported_sizes[0],
               supported_sizes[1]});
}

// Check that if the implementation supports two sizes that have the same
// resultant scale factor, then the widest one is selected.
TEST_F(ResizingHostObserverTest, SelectWidest) {
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(640, 480),
                                                   MakeResolution(480, 640)};
  InitDesktopResizer(MakeResolution(480, 640), false, supported_sizes, true);
  VerifySizes({MakeResolution(100, 100),
               MakeResolution(480, 480),
               MakeResolution(500, 500),
               MakeResolution(640, 640),
               MakeResolution(1000, 1000)},
              {supported_sizes[0],
               supported_sizes[0],
               supported_sizes[0],
               supported_sizes[0],
               supported_sizes[0]});
}

// Check that if the best match for the client size doesn't change, then we
// don't call SetSize.
TEST_F(ResizingHostObserverTest, NoSetSizeForSameSize) {
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(640, 480),
                                                   MakeResolution(480, 640)};
  InitDesktopResizer(MakeResolution(480, 640), false, supported_sizes, true);
  VerifySizes({MakeResolution(640, 640),
               MakeResolution(1024, 768),
               MakeResolution(640, 480)},
              {supported_sizes[0],
               supported_sizes[0],
               supported_sizes[0]});
  EXPECT_EQ(1, call_counts_.set_resolution);
}

// Check that desktop resizes are rate-limited, and that if multiple resize
// requests are received in the time-out period, the most recent is respected.
TEST_F(ResizingHostObserverTest, RateLimited) {
  InitDesktopResizer(MakeResolution(640, 480), true,
                     std::vector<ScreenResolution>(), true);
  resizing_host_observer_->SetNowFunctionForTesting(
      base::Bind(&ResizingHostObserverTest::GetTime, base::Unretained(this)));

  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  EXPECT_EQ(MakeResolution(100, 100),
            GetBestResolution(MakeResolution(100, 100)));
  now_ += base::TimeDelta::FromMilliseconds(900);
  EXPECT_EQ(MakeResolution(100, 100),
            GetBestResolution(MakeResolution(200, 200)));
  now_ += base::TimeDelta::FromMilliseconds(99);
  EXPECT_EQ(MakeResolution(100, 100),
            GetBestResolution(MakeResolution(300, 300)));
  now_ += base::TimeDelta::FromMilliseconds(1);

  // Due to the kMinimumResizeIntervalMs constant in resizing_host_observer.cc,
  // We need to wait a total of 1000ms for the final resize to be processed.
  // Since it was queued 900 + 99 ms after the first, we need to wait an
  // additional 1ms. However, since RunLoop is not guaranteed to process tasks
  // with the same due time in FIFO order, wait an additional 1ms for safety.
  task_environment.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(2));
  run_loop.Run();

  // If the QuitClosure fired before the final resize, it's a test failure.
  EXPECT_EQ(MakeResolution(300, 300), current_resolution_);
}

}  // namespace remoting
