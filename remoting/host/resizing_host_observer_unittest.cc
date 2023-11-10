// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resizing_host_observer.h"

#include <list>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_resizer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

using Monitors = std::map<webrtc::ScreenId, ScreenResolution>;

std::ostream& operator<<(std::ostream& os, const ScreenResolution& resolution) {
  return os << resolution.dimensions().width() << "x"
            << resolution.dimensions().height() << " @ " << resolution.dpi().x()
            << "x" << resolution.dpi().y();
}

bool operator==(const ScreenResolution& a, const ScreenResolution& b) {
  return a.Equals(b);
}

const int kDefaultDPI = 96;

ScreenResolution MakeResolution(int width, int height) {
  return ScreenResolution(webrtc::DesktopSize(width, height),
                          webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
}

// Converts a monitor-list to an object suitable for passing to
// ResizingHostObserver::OnDisplayInfoChanged().
DesktopDisplayInfo ToDisplayInfo(const Monitors& monitors) {
  DesktopDisplayInfo result;
  for (const auto& [id, resolution] : monitors) {
    DisplayGeometry geo = {
        .id = id,
        .x = 0,
        .y = 0,
        .width = static_cast<uint32_t>(resolution.dimensions().width()),
        .height = static_cast<uint32_t>(resolution.dimensions().height()),
        .dpi = static_cast<uint32_t>(resolution.dpi().x()),
        .bpp = 32,
        .is_default = false};
    result.AddDisplay(geo);
  }
  return result;
}

class FakeDesktopResizer : public DesktopResizer {
 public:
  struct CallCounts {
    int set_resolution = 0;
    int restore_resolution = 0;
  };

  FakeDesktopResizer(bool exact_size_supported,
                     std::vector<ScreenResolution> supported_resolutions,
                     Monitors* monitors,
                     CallCounts* call_counts,
                     bool check_final_resolution)
      : exact_size_supported_(exact_size_supported),
        initial_resolutions_(*monitors),
        monitors_(monitors),
        supported_resolutions_(std::move(supported_resolutions)),
        call_counts_(call_counts),
        check_final_resolution_(check_final_resolution) {}

  ~FakeDesktopResizer() override {
    if (check_final_resolution_) {
      EXPECT_EQ(initial_resolutions_, *monitors_);
    }
  }

  // remoting::DesktopResizer interface
  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override {
    ExpectValidId(screen_id);
    return (*monitors_)[screen_id];
  }
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override {
    ExpectValidId(screen_id);
    std::list<ScreenResolution> result(supported_resolutions_.begin(),
                                       supported_resolutions_.end());
    if (exact_size_supported_) {
      result.push_back(preferred);
    }
    return result;
  }
  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override {
    ExpectValidId(screen_id);
    (*monitors_)[screen_id] = resolution;
    ++call_counts_->set_resolution;
  }
  void RestoreResolution(const ScreenResolution& resolution,
                         webrtc::ScreenId screen_id) override {
    ExpectValidId(screen_id);
    (*monitors_)[screen_id] = resolution;
    ++call_counts_->restore_resolution;
  }
  void SetVideoLayout(const protocol::VideoLayout& layout) override {
    NOTIMPLEMENTED();
  }

 private:
  // Fails the unittest if |screen_id| is not a valid monitor ID.
  void ExpectValidId(webrtc::ScreenId screen_id) {
    EXPECT_TRUE(base::Contains(*monitors_, screen_id));
  }

  bool exact_size_supported_;

  // Used to verify that |monitors_| was restored to the initial resolutions.
  Monitors initial_resolutions_;

  // List of monitors to be resized.
  raw_ptr<Monitors> monitors_;

  std::vector<ScreenResolution> supported_resolutions_;
  raw_ptr<CallCounts> call_counts_;
  bool check_final_resolution_;
};

class ResizingHostObserverTest : public testing::Test {
 public:
  ResizingHostObserverTest() { clock_.SetNowTicks(base::TimeTicks::Now()); }

 protected:
  void InitDesktopResizer(const Monitors& initial_resolutions,
                          bool exact_size_supported,
                          std::vector<ScreenResolution> supported_resolutions,
                          bool restore_resolution) {
    monitors_ = initial_resolutions;
    call_counts_ = FakeDesktopResizer::CallCounts();
    resizing_host_observer_ = std::make_unique<ResizingHostObserver>(
        std::make_unique<FakeDesktopResizer>(
            exact_size_supported, std::move(supported_resolutions), &monitors_,
            &call_counts_, restore_resolution),
        restore_resolution);
    resizing_host_observer_->SetClockForTesting(&clock_);
  }

  void SetScreenResolution(const ScreenResolution& client_size) {
    resizing_host_observer_->SetScreenResolution(client_size, std::nullopt);
    if (auto_advance_clock_) {
      clock_.Advance(base::Seconds(1));
    }
  }

  void SetScreenResolution(const ScreenResolution& client_size,
                           webrtc::ScreenId id) {
    resizing_host_observer_->SetScreenResolution(client_size, id);
    if (auto_advance_clock_) {
      clock_.Advance(base::Seconds(1));
    }
  }

  // Should be used only for single-monitor tests.
  ScreenResolution GetBestResolution(const ScreenResolution& client_size) {
    SetScreenResolution(client_size);
    return monitors_.begin()->second;
  }

  // Should be used only for single-monitor tests.
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

  // Sends the current display-info to the ResizingHostObserver.
  void NotifyDisplayInfo() {
    resizing_host_observer_->SetDisplayInfoForTesting(ToDisplayInfo(monitors_));
  }

  Monitors monitors_;
  FakeDesktopResizer::CallCounts call_counts_;
  std::unique_ptr<ResizingHostObserver> resizing_host_observer_;
  base::SimpleTestTickClock clock_;
  bool auto_advance_clock_ = true;
};

// Check that the resolution isn't restored if it wasn't changed by this class.
TEST_F(ResizingHostObserverTest, NoRestoreResolution) {
  InitDesktopResizer({{123, MakeResolution(640, 480)}}, false,
                     std::vector<ScreenResolution>(), true);
  NotifyDisplayInfo();
  resizing_host_observer_.reset();
  EXPECT_EQ(0, call_counts_.restore_resolution);
}

// Check that the host is not resized if GetSupportedSizes returns an empty
// list (even if GetCurrentSize is supported).
TEST_F(ResizingHostObserverTest, EmptyGetSupportedSizes) {
  ScreenResolution initial = MakeResolution(640, 480);
  InitDesktopResizer({{123, initial}}, false, std::vector<ScreenResolution>(),
                     true);
  NotifyDisplayInfo();
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
  InitDesktopResizer({{123, initial}}, false, supported_sizes, false);
  NotifyDisplayInfo();
  VerifySizes(client_sizes, client_sizes);
  resizing_host_observer_.reset();
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(0, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(1024, 768), monitors_[123]);

  // Flag true
  InitDesktopResizer({{123, initial}}, false, supported_sizes, true);
  NotifyDisplayInfo();
  VerifySizes(client_sizes, client_sizes);
  resizing_host_observer_.reset();
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(1, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(640, 480), monitors_[123]);
}

// Check that the size is restored if an empty ClientResolution is received.
TEST_F(ResizingHostObserverTest, RestoreOnEmptyClientResolution) {
  InitDesktopResizer({{123, MakeResolution(640, 480)}}, true,
                     std::vector<ScreenResolution>(), true);
  NotifyDisplayInfo();
  SetScreenResolution(MakeResolution(200, 100), 123);
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(0, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(200, 100), monitors_[123]);
  SetScreenResolution(MakeResolution(0, 0), 123);
  EXPECT_EQ(1, call_counts_.set_resolution);
  EXPECT_EQ(1, call_counts_.restore_resolution);
  EXPECT_EQ(MakeResolution(640, 480), monitors_[123]);
}

// Check that if the implementation supports exact size matching, it is used.
TEST_F(ResizingHostObserverTest, SelectExactSize) {
  InitDesktopResizer({{123, MakeResolution(640, 480)}}, true,
                     std::vector<ScreenResolution>(), true);
  NotifyDisplayInfo();
  std::vector<ScreenResolution> client_sizes = {
      MakeResolution(200, 100), MakeResolution(100, 200),
      MakeResolution(640, 480), MakeResolution(480, 640),
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
  InitDesktopResizer({{123, MakeResolution(640, 480)}}, false, supported_sizes,
                     true);
  NotifyDisplayInfo();
  VerifySizes({MakeResolution(639, 479), MakeResolution(640, 480),
               MakeResolution(641, 481), MakeResolution(999, 999)},
              {supported_sizes[0], supported_sizes[1], supported_sizes[1],
               supported_sizes[1]});
}

// Check that if the implementation supports only sizes that are larger than
// the requested size, then the one that requires the least down-scaling.
TEST_F(ResizingHostObserverTest, SelectBestScaleFactor) {
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(100, 100),
                                                   MakeResolution(200, 100)};
  InitDesktopResizer({{123, MakeResolution(200, 100)}}, false, supported_sizes,
                     true);
  NotifyDisplayInfo();
  VerifySizes(
      {MakeResolution(1, 1), MakeResolution(99, 99), MakeResolution(199, 99)},
      {supported_sizes[0], supported_sizes[0], supported_sizes[1]});
}

// Check that if the implementation supports two sizes that have the same
// resultant scale factor, then the widest one is selected.
TEST_F(ResizingHostObserverTest, SelectWidest) {
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(640, 480),
                                                   MakeResolution(480, 640)};
  InitDesktopResizer({{123, MakeResolution(480, 640)}}, false, supported_sizes,
                     true);
  NotifyDisplayInfo();
  VerifySizes({MakeResolution(100, 100), MakeResolution(480, 480),
               MakeResolution(500, 500), MakeResolution(640, 640),
               MakeResolution(1000, 1000)},
              {supported_sizes[0], supported_sizes[0], supported_sizes[0],
               supported_sizes[0], supported_sizes[0]});
}

// Check that if the best match for the client size doesn't change, then we
// don't call SetSize.
TEST_F(ResizingHostObserverTest, NoSetSizeForSameSize) {
  std::vector<ScreenResolution> supported_sizes = {MakeResolution(640, 480),
                                                   MakeResolution(480, 640)};
  InitDesktopResizer({{123, MakeResolution(480, 640)}}, false, supported_sizes,
                     true);
  NotifyDisplayInfo();
  VerifySizes({MakeResolution(640, 640), MakeResolution(1024, 768),
               MakeResolution(640, 480)},
              {supported_sizes[0], supported_sizes[0], supported_sizes[0]});
  EXPECT_EQ(1, call_counts_.set_resolution);
}

// Check that desktop resizes are rate-limited, and that if multiple resize
// requests are received in the time-out period, the most recent is respected.
TEST_F(ResizingHostObserverTest, RateLimited) {
  InitDesktopResizer({{123, MakeResolution(640, 480)}}, true,
                     std::vector<ScreenResolution>(), true);
  NotifyDisplayInfo();
  auto_advance_clock_ = false;

  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  EXPECT_EQ(MakeResolution(100, 100),
            GetBestResolution(MakeResolution(100, 100)));
  clock_.Advance(base::Milliseconds(900));
  EXPECT_EQ(MakeResolution(100, 100),
            GetBestResolution(MakeResolution(200, 200)));
  clock_.Advance(base::Milliseconds(99));
  EXPECT_EQ(MakeResolution(100, 100),
            GetBestResolution(MakeResolution(300, 300)));
  clock_.Advance(base::Milliseconds(1));

  // Due to the kMinimumResizeIntervalMs constant in resizing_host_observer.cc,
  // We need to wait a total of 1000ms for the final resize to be processed.
  // Since it was queued 900 + 99 ms after the first, we need to wait an
  // additional 1ms. However, since RunLoop is not guaranteed to process tasks
  // with the same due time in FIFO order, wait an additional 1ms for safety.
  task_environment.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
  run_loop.Run();

  // If the QuitClosure fired before the final resize, it's a test failure.
  EXPECT_EQ(MakeResolution(300, 300), monitors_[123]);
}

TEST_F(ResizingHostObserverTest, PendingResolutionAppliedToFirstMonitor) {
  // An anonymous resolution request should be remembered and applied as soon
  // as the first display-info is provided.
  InitDesktopResizer({{123, MakeResolution(640, 480)}}, true,
                     std::vector<ScreenResolution>(), false);
  SetScreenResolution(MakeResolution(200, 100));
  EXPECT_EQ(0, call_counts_.set_resolution);
  NotifyDisplayInfo();
  EXPECT_EQ(1, call_counts_.set_resolution);
  Monitors expected = {{123, MakeResolution(200, 100)}};
  EXPECT_EQ(monitors_, expected);
}

TEST_F(ResizingHostObserverTest, AnonymousRequestDroppedIfMultipleMonitors) {
  InitDesktopResizer(
      {{123, MakeResolution(640, 480)}, {234, MakeResolution(800, 600)}}, true,
      std::vector<ScreenResolution>(), false);
  NotifyDisplayInfo();
  SetScreenResolution(MakeResolution(200, 100));
  EXPECT_EQ(0, call_counts_.set_resolution);
}

TEST_F(ResizingHostObserverTest, RequestDroppedForUnknownMonitor) {
  InitDesktopResizer({{123, MakeResolution(640, 480)}}, true,
                     std::vector<ScreenResolution>(), false);
  NotifyDisplayInfo();
  SetScreenResolution(MakeResolution(200, 100), 234);
  EXPECT_EQ(0, call_counts_.set_resolution);
  SetScreenResolution(MakeResolution(200, 100), 123);
  EXPECT_EQ(1, call_counts_.set_resolution);
}

TEST_F(ResizingHostObserverTest, MultipleMonitorSizesRestored) {
  InitDesktopResizer({{123, MakeResolution(1230, 1230)},
                      {234, MakeResolution(2340, 2340)},
                      {345, MakeResolution(3450, 3450)}},
                     true, std::vector<ScreenResolution>(), false);
  NotifyDisplayInfo();

  SetScreenResolution(MakeResolution(999, 999), 123);
  SetScreenResolution(MakeResolution(999, 999), 234);
  SetScreenResolution(MakeResolution(999, 999), 345);
  EXPECT_EQ(3, call_counts_.set_resolution);

  SetScreenResolution({}, 123);
  SetScreenResolution({}, 345);
  EXPECT_EQ(2, call_counts_.restore_resolution);
  Monitors expected = {{123, MakeResolution(1230, 1230)},
                       {234, MakeResolution(999, 999)},
                       {345, MakeResolution(3450, 3450)}};
  EXPECT_EQ(monitors_, expected);
}

}  // namespace remoting
