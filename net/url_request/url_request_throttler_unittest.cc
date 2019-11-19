// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/metrics/histogram_samples.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/url_request/url_request_throttler_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

namespace {

const char kRequestThrottledHistogramName[] = "Throttling.RequestThrottled";

class MockURLRequestThrottlerEntry : public URLRequestThrottlerEntry {
 public:
  explicit MockURLRequestThrottlerEntry(
      URLRequestThrottlerManager* manager)
      : URLRequestThrottlerEntry(manager, std::string()),
        backoff_entry_(&backoff_policy_, &fake_clock_) {
    InitPolicy();
  }
  MockURLRequestThrottlerEntry(
      URLRequestThrottlerManager* manager,
      const TimeTicks& exponential_backoff_release_time,
      const TimeTicks& sliding_window_release_time,
      const TimeTicks& fake_now)
      : URLRequestThrottlerEntry(manager, std::string()),
        fake_clock_(fake_now),
        backoff_entry_(&backoff_policy_, &fake_clock_) {
    InitPolicy();

    set_exponential_backoff_release_time(exponential_backoff_release_time);
    set_sliding_window_release_time(sliding_window_release_time);
  }

  void InitPolicy() {
    // Some tests become flaky if we have jitter.
    backoff_policy_.jitter_factor = 0.0;

    // This lets us avoid having to make multiple failures initially (this
    // logic is already tested in the BackoffEntry unit tests).
    backoff_policy_.num_errors_to_ignore = 0;
  }

  const BackoffEntry* GetBackoffEntry() const override {
    return &backoff_entry_;
  }

  BackoffEntry* GetBackoffEntry() override { return &backoff_entry_; }

  void ResetToBlank(const TimeTicks& time_now) {
    fake_clock_.set_now(time_now);

    GetBackoffEntry()->Reset();
    set_sliding_window_release_time(time_now);
  }

  // Overridden for tests.
  TimeTicks ImplGetTimeNow() const override { return fake_clock_.NowTicks(); }

  void set_fake_now(const TimeTicks& now) { fake_clock_.set_now(now); }

  void set_exponential_backoff_release_time(const TimeTicks& release_time) {
    GetBackoffEntry()->SetCustomReleaseTime(release_time);
  }

  TimeTicks sliding_window_release_time() const {
    return URLRequestThrottlerEntry::sliding_window_release_time();
  }

  void set_sliding_window_release_time(const TimeTicks& release_time) {
    URLRequestThrottlerEntry::set_sliding_window_release_time(release_time);
  }

 protected:
  ~MockURLRequestThrottlerEntry() override = default;

 private:
  mutable TestTickClock fake_clock_;
  BackoffEntry backoff_entry_;
};

class MockURLRequestThrottlerManager : public URLRequestThrottlerManager {
 public:
  MockURLRequestThrottlerManager() : create_entry_index_(0) {}

  // Method to process the URL using URLRequestThrottlerManager protected
  // method.
  std::string DoGetUrlIdFromUrl(const GURL& url) { return GetIdFromUrl(url); }

  // Method to use the garbage collecting method of URLRequestThrottlerManager.
  void DoGarbageCollectEntries() { GarbageCollectEntries(); }

  // Returns the number of entries in the map.
  int GetNumberOfEntries() const { return GetNumberOfEntriesForTests(); }

  void CreateEntry(bool is_outdated) {
    TimeTicks time = TimeTicks::Now();
    if (is_outdated) {
      time -= TimeDelta::FromMilliseconds(
          MockURLRequestThrottlerEntry::kDefaultEntryLifetimeMs + 1000);
    }
    std::string fake_url_string("http://www.fakeurl.com/");
    fake_url_string.append(base::NumberToString(create_entry_index_++));
    GURL fake_url(fake_url_string);
    OverrideEntryForTests(
        fake_url,
        new MockURLRequestThrottlerEntry(this, time, TimeTicks::Now(),
                                         TimeTicks::Now()));
  }

 private:
  int create_entry_index_;
};

struct TimeAndBool {
  TimeAndBool(const TimeTicks& time_value, bool expected, int line_num) {
    time = time_value;
    result = expected;
    line = line_num;
  }
  TimeTicks time;
  bool result;
  int line;
};

struct GurlAndString {
  GurlAndString(const GURL& url_value,
                const std::string& expected,
                int line_num) {
    url = url_value;
    result = expected;
    line = line_num;
  }
  GURL url;
  std::string result;
  int line;
};

}  // namespace

class URLRequestThrottlerEntryTest : public TestWithTaskEnvironment {
 protected:
  URLRequestThrottlerEntryTest()
      : request_(context_.CreateRequest(GURL(),
                                        DEFAULT_PRIORITY,
                                        nullptr,
                                        TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  void SetUp() override;

  TimeTicks now_;
  MockURLRequestThrottlerManager manager_;  // Dummy object, not used.
  scoped_refptr<MockURLRequestThrottlerEntry> entry_;

  TestURLRequestContext context_;
  std::unique_ptr<URLRequest> request_;
};

void URLRequestThrottlerEntryTest::SetUp() {
  request_->SetLoadFlags(0);

  now_ = TimeTicks::Now();
  entry_ = new MockURLRequestThrottlerEntry(&manager_);
  entry_->ResetToBlank(now_);
}

std::ostream& operator<<(std::ostream& out, const base::TimeTicks& time) {
  return out << time.ToInternalValue();
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceDuringExponentialBackoff) {
  base::HistogramTester histogram_tester;
  entry_->set_exponential_backoff_release_time(
      entry_->ImplGetTimeNow() + TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(entry_->ShouldRejectRequest(*request_));

  histogram_tester.ExpectBucketCount(kRequestThrottledHistogramName, 0, 0);
  histogram_tester.ExpectBucketCount(kRequestThrottledHistogramName, 1, 1);
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceNotDuringExponentialBackoff) {
  base::HistogramTester histogram_tester;
  entry_->set_exponential_backoff_release_time(entry_->ImplGetTimeNow());
  EXPECT_FALSE(entry_->ShouldRejectRequest(*request_));
  entry_->set_exponential_backoff_release_time(
      entry_->ImplGetTimeNow() - TimeDelta::FromMilliseconds(1));
  EXPECT_FALSE(entry_->ShouldRejectRequest(*request_));

  histogram_tester.ExpectBucketCount(kRequestThrottledHistogramName, 0, 2);
  histogram_tester.ExpectBucketCount(kRequestThrottledHistogramName, 1, 0);
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateFailure) {
  entry_->UpdateWithResponse(503);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(),
            entry_->ImplGetTimeNow())
      << "A failure should increase the release_time";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccess) {
  entry_->UpdateWithResponse(200);
  EXPECT_EQ(entry_->GetExponentialBackoffReleaseTime(),
            entry_->ImplGetTimeNow())
      << "A success should not add any delay";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccessThenFailure) {
  entry_->UpdateWithResponse(200);
  entry_->UpdateWithResponse(503);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(),
            entry_->ImplGetTimeNow())
      << "This scenario should add delay";
  entry_->UpdateWithResponse(200);
}

TEST_F(URLRequestThrottlerEntryTest, IsEntryReallyOutdated) {
  TimeDelta lifetime = TimeDelta::FromMilliseconds(
      MockURLRequestThrottlerEntry::kDefaultEntryLifetimeMs);
  const TimeDelta kFiveMs = TimeDelta::FromMilliseconds(5);

  TimeAndBool test_values[] = {
      TimeAndBool(now_, false, __LINE__),
      TimeAndBool(now_ - kFiveMs, false, __LINE__),
      TimeAndBool(now_ + kFiveMs, false, __LINE__),
      TimeAndBool(now_ - (lifetime - kFiveMs), false, __LINE__),
      TimeAndBool(now_ - lifetime, true, __LINE__),
      TimeAndBool(now_ - (lifetime + kFiveMs), true, __LINE__)};

  for (unsigned int i = 0; i < base::size(test_values); ++i) {
    entry_->set_exponential_backoff_release_time(test_values[i].time);
    EXPECT_EQ(entry_->IsEntryOutdated(), test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(URLRequestThrottlerEntryTest, MaxAllowedBackoff) {
  for (int i = 0; i < 30; ++i) {
    entry_->UpdateWithResponse(503);
  }

  TimeDelta delay = entry_->GetExponentialBackoffReleaseTime() - now_;
  EXPECT_EQ(delay.InMilliseconds(),
            MockURLRequestThrottlerEntry::kDefaultMaximumBackoffMs);
}

TEST_F(URLRequestThrottlerEntryTest, MalformedContent) {
  for (int i = 0; i < 5; ++i)
    entry_->UpdateWithResponse(503);

  TimeTicks release_after_failures = entry_->GetExponentialBackoffReleaseTime();

  // Inform the entry that a response body was malformed, which is supposed to
  // increase the back-off time.  Note that we also submit a successful
  // UpdateWithResponse to pair with ReceivedContentWasMalformed() since that
  // is what happens in practice (if a body is received, then a non-500
  // response must also have been received).
  entry_->ReceivedContentWasMalformed(200);
  entry_->UpdateWithResponse(200);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), release_after_failures);
}

TEST_F(URLRequestThrottlerEntryTest, SlidingWindow) {
  int max_send = URLRequestThrottlerEntry::kDefaultMaxSendThreshold;
  int sliding_window =
      URLRequestThrottlerEntry::kDefaultSlidingWindowPeriodMs;

  TimeTicks time_1 = entry_->ImplGetTimeNow() +
      TimeDelta::FromMilliseconds(sliding_window / 3);
  TimeTicks time_2 = entry_->ImplGetTimeNow() +
      TimeDelta::FromMilliseconds(2 * sliding_window / 3);
  TimeTicks time_3 = entry_->ImplGetTimeNow() +
      TimeDelta::FromMilliseconds(sliding_window);
  TimeTicks time_4 = entry_->ImplGetTimeNow() +
      TimeDelta::FromMilliseconds(sliding_window + 2 * sliding_window / 3);

  entry_->set_exponential_backoff_release_time(time_1);

  for (int i = 0; i < max_send / 2; ++i) {
    EXPECT_EQ(2 * sliding_window / 3,
              entry_->ReserveSendingTimeForNextRequest(time_2));
  }
  EXPECT_EQ(time_2, entry_->sliding_window_release_time());

  entry_->set_fake_now(time_3);

  for (int i = 0; i < (max_send + 1) / 2; ++i)
    EXPECT_EQ(0, entry_->ReserveSendingTimeForNextRequest(TimeTicks()));

  EXPECT_EQ(time_4, entry_->sliding_window_release_time());
}

class URLRequestThrottlerManagerTest : public TestWithTaskEnvironment {
 protected:
  URLRequestThrottlerManagerTest()
      : request_(context_.CreateRequest(GURL(),
                                        DEFAULT_PRIORITY,
                                        nullptr,
                                        TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  void SetUp() override { request_->SetLoadFlags(0); }

  // context_ must be declared before request_.
  TestURLRequestContext context_;
  std::unique_ptr<URLRequest> request_;
};

TEST_F(URLRequestThrottlerManagerTest, IsUrlStandardised) {
  MockURLRequestThrottlerManager manager;
  GurlAndString test_values[] = {
      GurlAndString(GURL("http://www.example.com"),
                    std::string("http://www.example.com/"),
                    __LINE__),
      GurlAndString(GURL("http://www.Example.com"),
                    std::string("http://www.example.com/"),
                    __LINE__),
      GurlAndString(GURL("http://www.ex4mple.com/Pr4c71c41"),
                    std::string("http://www.ex4mple.com/pr4c71c41"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/0/token/false"),
                    std::string("http://www.example.com/0/token/false"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=javascript"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=1#superEntry"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php#superEntry"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com:1234/"),
                    std::string("http://www.example.com:1234/"),
                    __LINE__)};

  for (unsigned int i = 0; i < base::size(test_values); ++i) {
    std::string temp = manager.DoGetUrlIdFromUrl(test_values[i].url);
    EXPECT_EQ(temp, test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(URLRequestThrottlerManagerTest, AreEntriesBeingCollected) {
  MockURLRequestThrottlerManager manager;

  manager.CreateEntry(true);  // true = Entry is outdated.
  manager.CreateEntry(true);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(0, manager.GetNumberOfEntries());

  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

TEST_F(URLRequestThrottlerManagerTest, IsHostBeingRegistered) {
  MockURLRequestThrottlerManager manager;

  manager.RegisterRequestUrl(GURL("http://www.example.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0?code=1"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0#lolsaure"));

  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

TEST_F(URLRequestThrottlerManagerTest, LocalHostOptedOut) {
  MockURLRequestThrottlerManager manager;
  // A localhost entry should always be opted out.
  scoped_refptr<URLRequestThrottlerEntryInterface> localhost_entry =
      manager.RegisterRequestUrl(GURL("http://localhost/hello"));
  EXPECT_FALSE(localhost_entry->ShouldRejectRequest(*request_));
  for (int i = 0; i < 10; ++i) {
    localhost_entry->UpdateWithResponse(503);
  }
  EXPECT_FALSE(localhost_entry->ShouldRejectRequest(*request_));

  // We're not mocking out GetTimeNow() in this scenario
  // so add a 100 ms buffer to avoid flakiness (that should always
  // give enough time to get from the TimeTicks::Now() call here
  // to the TimeTicks::Now() call in the entry class).
  EXPECT_GT(TimeTicks::Now() + TimeDelta::FromMilliseconds(100),
            localhost_entry->GetExponentialBackoffReleaseTime());
}

TEST_F(URLRequestThrottlerManagerTest, ClearOnNetworkChange) {
  for (int i = 0; i < 3; ++i) {
    MockURLRequestThrottlerManager manager;
    scoped_refptr<URLRequestThrottlerEntryInterface> entry_before =
        manager.RegisterRequestUrl(GURL("http://www.example.com/"));
    for (int j = 0; j < 10; ++j) {
      entry_before->UpdateWithResponse(503);
    }
    EXPECT_TRUE(entry_before->ShouldRejectRequest(*request_));

    switch (i) {
      case 0:
        manager.OnIPAddressChanged();
        break;
      case 1:
        manager.OnConnectionTypeChanged(
            NetworkChangeNotifier::CONNECTION_UNKNOWN);
        break;
      case 2:
        manager.OnConnectionTypeChanged(NetworkChangeNotifier::CONNECTION_NONE);
        break;
      default:
        FAIL();
    }

    scoped_refptr<URLRequestThrottlerEntryInterface> entry_after =
        manager.RegisterRequestUrl(GURL("http://www.example.com/"));
    EXPECT_FALSE(entry_after->ShouldRejectRequest(*request_));
  }
}

}  // namespace net
