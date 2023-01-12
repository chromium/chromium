// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/upload_progress_tracker.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/upload_progress.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class TestingUploadProgressTracker : public UploadProgressTracker {
 public:
  TestingUploadProgressTracker(
      const base::Location& location,
      UploadProgressReportCallback report_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : UploadProgressTracker(location,
                              std::move(report_callback),
                              nullptr,
                              std::move(task_runner)),
        current_time_(base::TimeTicks::Now()) {}

  TestingUploadProgressTracker(const TestingUploadProgressTracker&) = delete;
  TestingUploadProgressTracker& operator=(const TestingUploadProgressTracker&) =
      delete;

  void set_upload_progress(const net::UploadProgress& upload_progress) {
    upload_progress_ = upload_progress;
  }

  void set_current_time(const base::TimeTicks& current_time) {
    current_time_ = current_time;
  }

 private:
  // UploadProgressTracker overrides.
  base::TimeTicks GetCurrentTime() const override { return current_time_; }
  net::UploadProgress GetUploadProgress() const override {
    return upload_progress_;
  }

  base::TimeTicks current_time_;
  net::UploadProgress upload_progress_;
};

}  // namespace

class UploadProgressTrackerTest : public ::testing::Test {
 public:
  UploadProgressTrackerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        upload_progress_tracker_(
            FROM_HERE,
            base::BindRepeating(
                &UploadProgressTrackerTest::OnUploadProgressReported,
                base::Unretained(this)),
            task_environment_.GetMainThreadTaskRunner()) {}

  UploadProgressTrackerTest(const UploadProgressTrackerTest&) = delete;
  UploadProgressTrackerTest& operator=(const UploadProgressTrackerTest&) =
      delete;

 private:
  void OnUploadProgressReported(const net::UploadProgress& progress) {
    ++report_count_;
    reported_position_ = progress.position();
    reported_total_size_ = progress.size();
  }

 protected:
  int report_count_ = 0;
  int64_t reported_position_ = 0;
  int64_t reported_total_size_ = 0;

  base::test::SingleThreadTaskEnvironment task_environment_;

  TestingUploadProgressTracker upload_progress_tracker_;
};

TEST_F(UploadProgressTrackerTest, NoACK) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.set_upload_progress(net::UploadProgress(750, 1000));

  // The second timer task does nothing, since the first report didn't send the
  // ACK.
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
}

TEST_F(UploadProgressTrackerTest, NoUpload) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(0, 0));

  // UploadProgressTracker does nothing on the empty upload content.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(0, report_count_);
}

TEST_F(UploadProgressTrackerTest, NoProgress) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();

  // The second time doesn't call ReportUploadProgress since there's no
  // progress.
  EXPECT_EQ(1, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
}

TEST_F(UploadProgressTrackerTest, Finished) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(999, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(999, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(1000, 1000));

  // The second timer task calls ReportUploadProgress for reporting the
  // completion.
  EXPECT_EQ(1, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(1000, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);
}

TEST_F(UploadProgressTrackerTest, Progress) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(750, 1000));

  // The second timer task calls ReportUploadProgress since the progress is
  // big enough to report.
  EXPECT_EQ(1, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(750, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);
}

TEST_F(UploadProgressTrackerTest, TimePassed) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(501, 1000));

  // The second timer task doesn't call ReportUploadProgress since the progress
  // is too small to report it.
  EXPECT_EQ(1, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);

  upload_progress_tracker_.set_current_time(base::TimeTicks::Now() +
                                            base::Seconds(5));

  // The third timer task calls ReportUploadProgress since it's been long time
  // from the last report.
  EXPECT_EQ(1, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(501, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);
}

TEST_F(UploadProgressTrackerTest, Rewound) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.OnAckReceived();
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(250, 1000));

  // The second timer task doesn't call ReportUploadProgress since the progress
  // was rewound.
  EXPECT_EQ(1, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);

  upload_progress_tracker_.set_current_time(base::TimeTicks::Now() +
                                            base::Seconds(5));

  // Even after a good amount of time passed, the rewound progress should not be
  // reported.
  EXPECT_EQ(1, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
}

TEST_F(UploadProgressTrackerTest, Completed) {
  upload_progress_tracker_.set_upload_progress(net::UploadProgress(500, 1000));

  // The first timer task calls ReportUploadProgress.
  EXPECT_EQ(0, report_count_);
  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(1, report_count_);
  EXPECT_EQ(500, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  upload_progress_tracker_.set_upload_progress(net::UploadProgress(1000, 1000));

  // OnUploadCompleted runs ReportUploadProgress even without Ack nor timer.
  upload_progress_tracker_.OnUploadCompleted();
  EXPECT_EQ(2, report_count_);
  EXPECT_EQ(1000, reported_position_);
  EXPECT_EQ(1000, reported_total_size_);

  task_environment_.FastForwardBy(
      UploadProgressTracker::GetUploadProgressIntervalForTesting());
  EXPECT_EQ(2, report_count_);
  EXPECT_TRUE(task_environment_.MainThreadIsIdle());
}

}  // namespace network
