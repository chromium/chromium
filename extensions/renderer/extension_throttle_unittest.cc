// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "extensions/renderer/extension_throttle_entry.h"
#include "extensions/renderer/extension_throttle_manager.h"
#include "extensions/renderer/extension_throttle_test_support.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;
using net::BackoffEntry;

namespace extensions {

namespace {

class MockExtensionThrottleEntry : public ExtensionThrottleEntry {
 public:
  MockExtensionThrottleEntry()
      : ExtensionThrottleEntry(std::string()),
        backoff_entry_(&backoff_policy_, &fake_clock_) {
    InitPolicy();
  }
  MockExtensionThrottleEntry(const TimeTicks& exponential_backoff_release_time,
                             const TimeTicks& sliding_window_release_time,
                             const TimeTicks& fake_now)
      : ExtensionThrottleEntry(std::string()),
        fake_clock_(fake_now),
        backoff_entry_(&backoff_policy_, &fake_clock_) {
    InitPolicy();

    set_exponential_backoff_release_time(exponential_backoff_release_time);
    set_sliding_window_release_time(sliding_window_release_time);
  }

  ~MockExtensionThrottleEntry() override {}

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
    return ExtensionThrottleEntry::sliding_window_release_time();
  }

  void set_sliding_window_release_time(const TimeTicks& release_time) {
    ExtensionThrottleEntry::set_sliding_window_release_time(release_time);
  }

 private:
  mutable TestTickClock fake_clock_;
  BackoffEntry backoff_entry_;
};

class MockExtensionThrottleManager : public ExtensionThrottleManager {
 public:
  MockExtensionThrottleManager() : create_entry_index_(0) {}

  std::string GetIdFromUrl(const GURL& url) const {
    return ExtensionThrottleManager::GetIdFromUrl(url);
  }

  ExtensionThrottleEntry* RegisterRequestUrl(const GURL& url) {
    return ExtensionThrottleManager::RegisterRequestUrl(url);
  }

  void GarbageCollectEntries() {
    ExtensionThrottleManager::GarbageCollectEntries();
  }

  // Returns the number of entries in the map.
  int GetNumberOfEntries() const { return GetNumberOfEntriesForTests(); }

  void CreateEntry(bool is_outdated) {
    TimeTicks time = TimeTicks::Now();
    if (is_outdated) {
      time -= base::Milliseconds(
          MockExtensionThrottleEntry::kDefaultEntryLifetimeMs + 1000);
    }
    std::string fake_url_string("http://www.fakeurl.com/");
    fake_url_string.append(base::NumberToString(create_entry_index_++));
    GURL fake_url(fake_url_string);
    OverrideEntryForTests(fake_url,
                          std::make_unique<MockExtensionThrottleEntry>(
                              time, TimeTicks::Now(), TimeTicks::Now()));
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

class ExtensionThrottleEntryTest : public testing::Test {
 protected:
  ExtensionThrottleEntryTest() = default;

  void SetUp() override;

  TimeTicks now_;
  MockExtensionThrottleManager manager_;  // Dummy object, not used.
  std::unique_ptr<MockExtensionThrottleEntry> entry_;
};

void ExtensionThrottleEntryTest::SetUp() {
  now_ = TimeTicks::Now();
  entry_ = std::make_unique<MockExtensionThrottleEntry>();
  entry_->ResetToBlank(now_);
}

TEST_F(ExtensionThrottleEntryTest, CanThrottleRequest) {
  entry_->set_exponential_backoff_release_time(entry_->ImplGetTimeNow() +
                                               base::Milliseconds(1));
  EXPECT_TRUE(entry_->ShouldRejectRequest());
}

TEST_F(ExtensionThrottleEntryTest,
       CanThrottleRequestNotDuringExponentialBackoff) {
  entry_->set_exponential_backoff_release_time(entry_->ImplGetTimeNow());
  EXPECT_FALSE(entry_->ShouldRejectRequest());
  entry_->set_exponential_backoff_release_time(entry_->ImplGetTimeNow() -
                                               base::Milliseconds(1));
  EXPECT_FALSE(entry_->ShouldRejectRequest());
}

TEST_F(ExtensionThrottleEntryTest, InterfaceUpdateFailure) {
  entry_->UpdateWithResponse(503);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(),
            entry_->ImplGetTimeNow())
      << "A failure should increase the release_time";
}

TEST_F(ExtensionThrottleEntryTest, InterfaceUpdateSuccess) {
  entry_->UpdateWithResponse(200);
  EXPECT_EQ(entry_->GetExponentialBackoffReleaseTime(),
            entry_->ImplGetTimeNow())
      << "A success should not add any delay";
}

TEST_F(ExtensionThrottleEntryTest, InterfaceUpdateSuccessThenFailure) {
  entry_->UpdateWithResponse(200);
  entry_->UpdateWithResponse(503);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(),
            entry_->ImplGetTimeNow())
      << "This scenario should add delay";
  entry_->UpdateWithResponse(200);
}

TEST_F(ExtensionThrottleEntryTest, IsEntryReallyOutdated) {
  base::TimeDelta lifetime =
      base::Milliseconds(MockExtensionThrottleEntry::kDefaultEntryLifetimeMs);
  const base::TimeDelta kFiveMs = base::Milliseconds(5);

  TimeAndBool test_values[] = {
      TimeAndBool(now_, false, __LINE__),
      TimeAndBool(now_ - kFiveMs, false, __LINE__),
      TimeAndBool(now_ + kFiveMs, false, __LINE__),
      TimeAndBool(now_ - (lifetime - kFiveMs), false, __LINE__),
      TimeAndBool(now_ - lifetime, true, __LINE__),
      TimeAndBool(now_ - (lifetime + kFiveMs), true, __LINE__)};

  for (unsigned int i = 0; i < std::size(test_values); ++i) {
    entry_->set_exponential_backoff_release_time(test_values[i].time);
    EXPECT_EQ(entry_->IsEntryOutdated(), test_values[i].result)
        << "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(ExtensionThrottleEntryTest, MaxAllowedBackoff) {
  for (int i = 0; i < 30; ++i) {
    entry_->UpdateWithResponse(503);
  }

  base::TimeDelta delay = entry_->GetExponentialBackoffReleaseTime() - now_;
  EXPECT_EQ(delay.InMilliseconds(),
            MockExtensionThrottleEntry::kDefaultMaximumBackoffMs);
}

TEST_F(ExtensionThrottleEntryTest, MalformedContent) {
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

TEST_F(ExtensionThrottleEntryTest, SlidingWindow) {
  int max_send = ExtensionThrottleEntry::kDefaultMaxSendThreshold;
  int sliding_window = ExtensionThrottleEntry::kDefaultSlidingWindowPeriodMs;

  TimeTicks time_1 =
      entry_->ImplGetTimeNow() + base::Milliseconds(sliding_window / 3);
  TimeTicks time_2 =
      entry_->ImplGetTimeNow() + base::Milliseconds(2 * sliding_window / 3);
  TimeTicks time_3 =
      entry_->ImplGetTimeNow() + base::Milliseconds(sliding_window);
  TimeTicks time_4 =
      entry_->ImplGetTimeNow() +
      base::Milliseconds(sliding_window + 2 * sliding_window / 3);

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

TEST(ExtensionThrottleManagerTest, IsUrlStandardised) {
  MockExtensionThrottleManager manager;
  GurlAndString test_values[] = {
      GurlAndString(GURL("http://www.example.com"),
                    std::string("http://www.example.com/"), __LINE__),
      GurlAndString(GURL("http://www.Example.com"),
                    std::string("http://www.example.com/"), __LINE__),
      GurlAndString(GURL("http://www.ex4mple.com/Pr4c71c41"),
                    std::string("http://www.ex4mple.com/pr4c71c41"), __LINE__),
      GurlAndString(GURL("http://www.example.com/0/token/false"),
                    std::string("http://www.example.com/0/token/false"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=javascript"),
                    std::string("http://www.example.com/index.php"), __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=1#superEntry"),
                    std::string("http://www.example.com/index.php"), __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php#superEntry"),
                    std::string("http://www.example.com/index.php"), __LINE__),
      GurlAndString(GURL("http://www.example.com:1234/"),
                    std::string("http://www.example.com:1234/"), __LINE__)};

  for (unsigned int i = 0; i < std::size(test_values); ++i) {
    std::string temp = manager.GetIdFromUrl(test_values[i].url);
    EXPECT_EQ(temp, test_values[i].result)
        << "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST(ExtensionThrottleManagerTest, AreEntriesBeingCollected) {
  MockExtensionThrottleManager manager;

  manager.CreateEntry(true);  // true = Entry is outdated.
  manager.CreateEntry(true);
  manager.CreateEntry(true);
  manager.GarbageCollectEntries();
  EXPECT_EQ(0, manager.GetNumberOfEntries());

  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(true);
  manager.GarbageCollectEntries();
  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

TEST(ExtensionThrottleManagerTest, IsHostBeingRegistered) {
  MockExtensionThrottleManager manager;

  manager.RegisterRequestUrl(GURL("http://www.example.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0?code=1"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0#lolsaure"));

  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

TEST(ExtensionThrottleManagerTest, LocalHostOptedOut) {
  MockExtensionThrottleManager manager;
  // A localhost entry should always be opted out.
  ExtensionThrottleEntry* localhost_entry =
      manager.RegisterRequestUrl(GURL("http://localhost/hello"));
  EXPECT_FALSE(localhost_entry->ShouldRejectRequest());
  for (int i = 0; i < 10; ++i) {
    localhost_entry->UpdateWithResponse(503);
  }
  EXPECT_FALSE(localhost_entry->ShouldRejectRequest());

  // We're not mocking out GetTimeNow() in this scenario
  // so add a 100 ms buffer to avoid flakiness (that should always
  // give enough time to get from the TimeTicks::Now() call here
  // to the TimeTicks::Now() call in the entry class).
  EXPECT_GT(TimeTicks::Now() + base::Milliseconds(100),
            localhost_entry->GetExponentialBackoffReleaseTime());
}

TEST(ExtensionThrottleManagerTest, ClearOnNetworkChange) {
  for (int i = 0; i < 2; ++i) {
    MockExtensionThrottleManager manager;
    ExtensionThrottleEntry* entry_before =
        manager.RegisterRequestUrl(GURL("http://www.example.com/"));
    for (int j = 0; j < 10; ++j) {
      entry_before->UpdateWithResponse(503);
    }
    EXPECT_TRUE(entry_before->ShouldRejectRequest());

    switch (i) {
      case 0:
        manager.SetOnline(/*is_online=*/true);
        break;
      case 1:
        manager.SetOnline(/*is_online=*/false);
        break;
      default:
        FAIL();
    }

    ExtensionThrottleEntry* entry_after =
        manager.RegisterRequestUrl(GURL("http://www.example.com/"));
    EXPECT_FALSE(entry_after->ShouldRejectRequest());
  }
}

TEST(ExtensionThrottleManagerTest, UseAfterNetworkChange) {
  MockExtensionThrottleManager manager;
  const GURL test_url("http://www.example.com/");
  EXPECT_FALSE(manager.ShouldRejectRequest(test_url));
  manager.SetOnline(/*is_online=*/false);
  manager.SetOnline(/*is_online=*/true);
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("http://www.newsite.com");
  EXPECT_FALSE(manager.ShouldRejectRedirect(test_url, redirect_info));
  manager.SetOnline(/*is_online=*/false);
  manager.SetOnline(/*is_online=*/true);
  auto response_head = network::mojom::URLResponseHead::New();
  manager.WillProcessResponse(redirect_info.new_url, *response_head);
}

}  // namespace extensions
