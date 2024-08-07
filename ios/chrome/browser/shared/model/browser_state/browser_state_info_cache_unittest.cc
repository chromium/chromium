// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"

#import "base/scoped_observation.h"
#import "base/time/time.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache_observer.h"
#import "testing/platform_test.h"

namespace {

// Constants used by tests.
struct TestAccount {
  std::string_view name;
  std::string_view gaia;
  std::string_view email;
  bool authenticated;
  base::Time last_active_time;
};

constexpr TestAccount kTestAccounts[] = {
    {
        .name = "Profile1",
        .gaia = "Gaia1",
        .email = "email1@example.com",
        .authenticated = true,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(1),
    },
    {
        .name = "Profile2",
        .gaia = "Gaia2",
        .email = "",
        .authenticated = true,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(2),
    },
    {
        .name = "Profile3",
        .gaia = "",
        .email = "email3@example.com",
        .authenticated = true,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(3),
    },
    {
        .name = "Profile4",
        .gaia = "",
        .email = "",
        .authenticated = false,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(4),
    },
};

// An observer that count how many times its events are invoked.
class BrowserStateInfoCacheTestObserver final
    : public BrowserStateInfoCacheObserver {
 public:
  explicit BrowserStateInfoCacheTestObserver(BrowserStateInfoCache& cache) {
    scoped_observation_.Observe(&cache);
  }

  ~BrowserStateInfoCacheTestObserver() override = default;

  size_t browser_state_added_count() const {
    return browser_state_added_count_;
  }

  size_t browser_state_removed_count() const {
    return browser_state_removed_count_;
  }

  // BrowserStateInfoCacheObserver:
  void OnBrowserStateAdded(std::string_view name) final {
    ++browser_state_added_count_;
  }

  void OnBrowserStateWasRemoved(std::string_view name) final {
    ++browser_state_removed_count_;
  }

 private:
  size_t browser_state_added_count_ = 0;
  size_t browser_state_removed_count_ = 0;

  base::ScopedObservation<BrowserStateInfoCache, BrowserStateInfoCacheObserver>
      scoped_observation_{this};
};

}  // namespace

class BrowserStateInfoCacheTest : public PlatformTest {
 public:
  BrowserStateInfoCacheTest() {
    BrowserStateInfoCache::RegisterPrefs(testing_pref_service_.registry());
  }

  PrefService* pref_service() { return &testing_pref_service_; }

 private:
  TestingPrefServiceSimple testing_pref_service_;
};

// Tests that AddBrowserState(...) inserts data for a BrowserState and
// notify the observers.
TEST_F(BrowserStateInfoCacheTest, AddBrowserState) {
  BrowserStateInfoCache cache(pref_service());
  BrowserStateInfoCacheTestObserver observer(cache);

  for (const TestAccount& account : kTestAccounts) {
    EXPECT_EQ(cache.GetIndexOfBrowserStateWithName(account.name),
              std::string::npos);

    cache.AddBrowserState(account.name, account.gaia, account.email);

    const size_t index = cache.GetIndexOfBrowserStateWithName(account.name);
    EXPECT_NE(index, std::string::npos);

    EXPECT_EQ(cache.GetNameOfBrowserStateAtIndex(index), account.name);
    EXPECT_EQ(cache.GetGAIAIdOfBrowserStateAtIndex(index), account.gaia);
    EXPECT_EQ(cache.GetUserNameOfBrowserStateAtIndex(index), account.email);
    EXPECT_EQ(cache.BrowserStateIsAuthenticatedAtIndex(index),
              account.authenticated);
  }

  // There is no duplicate, so there should be exactly as many BrowserState
  // known to the cache as there are test accounts.
  EXPECT_EQ(cache.GetNumberOfBrowserStates(), std::size(kTestAccounts));

  // The observer method OnBrowserStateAdded(...) should be called once for
  // each BrowserState, and the method OnBrowserStateWasRemoved(...) must
  // not have been called.
  EXPECT_EQ(observer.browser_state_added_count(), std::size(kTestAccounts));
  EXPECT_EQ(observer.browser_state_removed_count(), 0u);
}

// Tests that RemoveBrowserState(...) removes data for a BrowserState and
// notify the observers.
TEST_F(BrowserStateInfoCacheTest, RemoveBrowserState) {
  BrowserStateInfoCache cache(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    cache.AddBrowserState(account.name, account.gaia, account.email);
  }

  // There is no duplicate, so there should be exactly as many BrowserState
  // known to the cache as there are test accounts.
  EXPECT_EQ(cache.GetNumberOfBrowserStates(), std::size(kTestAccounts));

  BrowserStateInfoCacheTestObserver observer(cache);
  for (const TestAccount& account : kTestAccounts) {
    EXPECT_NE(cache.GetIndexOfBrowserStateWithName(account.name),
              std::string::npos);

    cache.RemoveBrowserState(account.name);

    EXPECT_EQ(cache.GetIndexOfBrowserStateWithName(account.name),
              std::string::npos);
  }

  // The observer method OnBrowserStateWasRemoved(...) should be called once
  // for each BrowserState, and the method OnBrowserStateAdded(...) must
  // not have been called.
  EXPECT_EQ(observer.browser_state_added_count(), 0u);
  EXPECT_EQ(observer.browser_state_removed_count(), std::size(kTestAccounts));
}

// Test the BrowserStateInfoCache saves the data to PrefService and can
// later load it correctly.
TEST_F(BrowserStateInfoCacheTest, PrefService) {
  // Add data to a first BrowserStateInfoCache, it should store the
  // data in the PrefService.
  {
    BrowserStateInfoCache cache(pref_service());
    for (const TestAccount& account : kTestAccounts) {
      cache.AddBrowserState(account.name, account.gaia, account.email);
      cache.SetLastActiveTimeOfBrowserStateAtIndex(
          cache.GetNumberOfBrowserStates() - 1, account.last_active_time);
    }
  }

  // Create a new BrowserStateInfoCache and check that it loads the
  // data from the PrefService correctly.
  const BrowserStateInfoCache cache(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    const size_t index = cache.GetIndexOfBrowserStateWithName(account.name);
    EXPECT_NE(index, std::string::npos);

    EXPECT_EQ(cache.GetNameOfBrowserStateAtIndex(index), account.name);
    EXPECT_EQ(cache.GetGAIAIdOfBrowserStateAtIndex(index), account.gaia);
    EXPECT_EQ(cache.GetUserNameOfBrowserStateAtIndex(index), account.email);
    EXPECT_EQ(cache.BrowserStateIsAuthenticatedAtIndex(index),
              account.authenticated);
    EXPECT_EQ(cache.GetLastActiveTimeOfBrowserStateAtIndex(index),
              account.last_active_time);
  }
}

// Tests that the saved browser state can be retrieve with the scene ID.
TEST_F(BrowserStateInfoCacheTest, MapBrowserStateAndSceneID) {
  BrowserStateInfoCache cache(pref_service());
  BrowserStateInfoCacheTestObserver observer(cache);

  std::string sceneID = "Test Scene ID";

  ASSERT_EQ(cache.GetBrowserStateNameForSceneID(sceneID), std::string());

  for (const TestAccount& account : kTestAccounts) {
    EXPECT_NE(cache.GetBrowserStateNameForSceneID(sceneID), account.name);
    cache.SetBrowserStateForSceneID(sceneID, account.name);
    EXPECT_EQ(cache.GetBrowserStateNameForSceneID(sceneID), account.name);
  }

  cache.ClearBrowserStateForSceneID(sceneID);
  EXPECT_EQ(cache.GetBrowserStateNameForSceneID(sceneID), std::string());
}

TEST_F(BrowserStateInfoCacheTest, SetAndGetLastActiveTime) {
  BrowserStateInfoCache cache(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    cache.AddBrowserState(account.name, account.gaia, account.email);
  }

  // The last-active time is initially unset.
  EXPECT_EQ(cache.GetLastActiveTimeOfBrowserStateAtIndex(0), base::Time());

  // Once set, it can be queried again.
  const base::Time time0 = base::Time::UnixEpoch() + base::Minutes(1);
  cache.SetLastActiveTimeOfBrowserStateAtIndex(0, time0);
  EXPECT_EQ(cache.GetLastActiveTimeOfBrowserStateAtIndex(0), time0);

  // Different BrowserStates do not affect each other.
  const base::Time time1 = base::Time::UnixEpoch() + base::Minutes(2);
  EXPECT_EQ(cache.GetLastActiveTimeOfBrowserStateAtIndex(1), base::Time());
  cache.SetLastActiveTimeOfBrowserStateAtIndex(1, time1);
  EXPECT_EQ(cache.GetLastActiveTimeOfBrowserStateAtIndex(1), time1);
  EXPECT_NE(cache.GetLastActiveTimeOfBrowserStateAtIndex(0),
            cache.GetLastActiveTimeOfBrowserStateAtIndex(1));
}
