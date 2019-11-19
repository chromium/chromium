// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/expiring_cache.h"

#include <functional>
#include <string>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Pointee;
using testing::StrEq;

namespace net {

namespace {

const int kMaxCacheEntries = 10;
typedef ExpiringCache<std::string, std::string, base::TimeTicks,
                      std::less<base::TimeTicks> > Cache;

struct TestFunctor {
  bool operator()(const std::string& now,
                  const std::string& expiration) const {
    return now != expiration;
  }
};

}  // namespace

TEST(ExpiringCacheTest, Basic) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  Cache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;
  EXPECT_EQ(0U, cache.size());

  // Add an entry at t=0
  EXPECT_FALSE(cache.Get("entry1", now));
  cache.Put("entry1", "test1", now, now + kTTL);
  EXPECT_THAT(cache.Get("entry1", now), Pointee(StrEq("test1")));
  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  // Add an entry at t=5.
  EXPECT_FALSE(cache.Get("entry2", now));
  cache.Put("entry2", "test2", now, now + kTTL);
  EXPECT_THAT(cache.Get("entry2", now), Pointee(StrEq("test2")));
  EXPECT_EQ(2U, cache.size());

  // Advance to t=9.
  now += base::TimeDelta::FromSeconds(4);

  // Verify that the entries added are still retrievable and usable.
  EXPECT_THAT(cache.Get("entry1", now), Pointee(StrEq("test1")));
  EXPECT_THAT(cache.Get("entry2", now), Pointee(StrEq("test2")));

  // Advance to t=10; entry1 is now expired.
  now += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(cache.Get("entry1", now));
  EXPECT_THAT(cache.Get("entry2", now), Pointee(StrEq("test2")));

  // The expired element should no longer be in the cache.
  EXPECT_EQ(1U, cache.size());

  // Update entry1 so it is no longer expired.
  cache.Put("entry1", "test1", now, now + kTTL);

  // Both entries should be retrievable and usable.
  EXPECT_EQ(2U, cache.size());
  EXPECT_THAT(cache.Get("entry1", now), Pointee(StrEq("test1")));
  EXPECT_THAT(cache.Get("entry2", now), Pointee(StrEq("test2")));

  // Advance to t=20; both entries are now expired.
  now += base::TimeDelta::FromSeconds(10);

  EXPECT_FALSE(cache.Get("entry1", now));
  EXPECT_FALSE(cache.Get("entry2", now));
}

TEST(ExpiringCacheTest, Compact) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  Cache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;
  EXPECT_EQ(0U, cache.size());

  // Add five valid entries at t=10 that expire at t=20.
  base::TimeTicks t10 = now + kTTL;
  for (int i = 0; i < 5; ++i) {
    std::string name = base::StringPrintf("valid%d", i);
    cache.Put(name, "I'm valid!", t10, t10 + kTTL);
  }
  EXPECT_EQ(5U, cache.size());

  // Add three entries at t=0 that expire at t=10.
  for (int i = 0; i < 3; ++i) {
    std::string name = base::StringPrintf("expired%d", i);
    cache.Put(name, "I'm expired.", now, t10);
  }
  EXPECT_EQ(8U, cache.size());

  // Add two negative (instantly expired) entries at t=0 that expire at t=0.
  for (int i = 0; i < 2; ++i) {
    std::string name = base::StringPrintf("negative%d", i);
    cache.Put(name, "I was never valid.", now, now);
  }
  EXPECT_EQ(10U, cache.size());

  EXPECT_TRUE(base::Contains(cache.entries_, "valid0"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid1"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid2"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid3"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid4"));
  EXPECT_TRUE(base::Contains(cache.entries_, "expired0"));
  EXPECT_TRUE(base::Contains(cache.entries_, "expired1"));
  EXPECT_TRUE(base::Contains(cache.entries_, "expired2"));
  EXPECT_TRUE(base::Contains(cache.entries_, "negative0"));
  EXPECT_TRUE(base::Contains(cache.entries_, "negative1"));

  // Shrink the new max constraints bound and compact. The "negative" and
  // "expired" entries should be dropped.
  cache.max_entries_ = 6;
  cache.Compact(now);
  EXPECT_EQ(5U, cache.size());

  EXPECT_TRUE(base::Contains(cache.entries_, "valid0"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid1"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid2"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid3"));
  EXPECT_TRUE(base::Contains(cache.entries_, "valid4"));
  EXPECT_FALSE(base::Contains(cache.entries_, "expired0"));
  EXPECT_FALSE(base::Contains(cache.entries_, "expired1"));
  EXPECT_FALSE(base::Contains(cache.entries_, "expired2"));
  EXPECT_FALSE(base::Contains(cache.entries_, "negative0"));
  EXPECT_FALSE(base::Contains(cache.entries_, "negative1"));

  // Shrink further -- this time the compact will start dropping valid entries
  // to make space.
  cache.max_entries_ = 4;
  cache.Compact(now);
  EXPECT_EQ(3U, cache.size());
}

// Add entries while the cache is at capacity, causing evictions.
TEST(ExpiringCacheTest, SetWithCompact) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  Cache cache(3);

  // t=10
  base::TimeTicks now = base::TimeTicks() + kTTL;

  cache.Put("test1", "test1", now, now + kTTL);
  cache.Put("test2", "test2", now, now + kTTL);
  cache.Put("expired", "expired", now, now);

  EXPECT_EQ(3U, cache.size());

  // Should all be retrievable except "expired".
  EXPECT_THAT(cache.Get("test1", now), Pointee(StrEq("test1")));
  EXPECT_THAT(cache.Get("test2", now), Pointee(StrEq("test2")));
  EXPECT_FALSE(cache.Get("expired", now));

  // Adding the fourth entry will cause "expired" to be evicted.
  cache.Put("test3", "test3", now, now + kTTL);
  EXPECT_EQ(3U, cache.size());

  EXPECT_FALSE(cache.Get("expired", now));
  EXPECT_THAT(cache.Get("test1", now), Pointee(StrEq("test1")));
  EXPECT_THAT(cache.Get("test2", now), Pointee(StrEq("test2")));
  EXPECT_THAT(cache.Get("test3", now), Pointee(StrEq("test3")));

  // Add two more entries. Something should be evicted, however "test5"
  // should definitely be in there (since it was last inserted).
  cache.Put("test4", "test4", now, now + kTTL);
  EXPECT_EQ(3U, cache.size());
  cache.Put("test5", "test5", now, now + kTTL);
  EXPECT_EQ(3U, cache.size());
  EXPECT_THAT(cache.Get("test5", now), Pointee(StrEq("test5")));
}

TEST(ExpiringCacheTest, Clear) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  Cache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;
  EXPECT_EQ(0U, cache.size());

  // Add three entries.
  cache.Put("test1", "foo", now, now + kTTL);
  cache.Put("test2", "foo", now, now + kTTL);
  cache.Put("test3", "foo", now, now + kTTL);
  EXPECT_EQ(3U, cache.size());

  cache.Clear();

  EXPECT_EQ(0U, cache.size());
}

TEST(ExpiringCacheTest, GetTruncatesExpiredEntries) {
  const base::TimeDelta kTTL = base::TimeDelta::FromSeconds(10);

  Cache cache(kMaxCacheEntries);

  // Start at t=0.
  base::TimeTicks now;
  EXPECT_EQ(0U, cache.size());

  // Add three entries at t=0.
  cache.Put("test1", "foo1", now, now + kTTL);
  cache.Put("test2", "foo2", now, now + kTTL);
  cache.Put("test3", "foo3", now, now + kTTL);
  EXPECT_EQ(3U, cache.size());

  // Ensure the entries were added.
  EXPECT_THAT(cache.Get("test1", now), Pointee(StrEq("foo1")));
  EXPECT_THAT(cache.Get("test2", now), Pointee(StrEq("foo2")));
  EXPECT_THAT(cache.Get("test3", now), Pointee(StrEq("foo3")));

  // Add five entries at t=10.
  now += kTTL;
  for (int i = 0; i < 5; ++i) {
    std::string name = base::StringPrintf("valid%d", i);
    cache.Put(name, name, now, now + kTTL);  // Expire at t=20.
  }
  EXPECT_EQ(8U, cache.size());

  // Now access two expired entries and ensure the cache size goes down.
  EXPECT_FALSE(cache.Get("test1", now));
  EXPECT_FALSE(cache.Get("test2", now));
  EXPECT_EQ(6U, cache.size());

  // Accessing non-expired entries should return entries and not adjust the
  // cache size.
  for (int i = 0; i < 5; ++i) {
    std::string name = base::StringPrintf("valid%d", i);
    EXPECT_THAT(cache.Get(name, now), Pointee(StrEq(name)));
  }
  EXPECT_EQ(6U, cache.size());
}

TEST(ExpiringCacheTest, CustomFunctor) {
  ExpiringCache<std::string, std::string, std::string, TestFunctor> cache(5);

  const std::string kNow("Now");
  const std::string kLater("A little bit later");
  const std::string kMuchLater("Much later");
  const std::string kHeatDeath("The heat death of the universe");

  EXPECT_EQ(0u, cache.size());

  // Add three entries at t=kNow that expire at kLater.
  cache.Put("test1", "foo1", kNow, kLater);
  cache.Put("test2", "foo2", kNow, kLater);
  cache.Put("test3", "foo3", kNow, kLater);
  EXPECT_EQ(3U, cache.size());

  // Add two entries at t=kNow that expire at kMuchLater
  cache.Put("test4", "foo4", kNow, kMuchLater);
  cache.Put("test5", "foo5", kNow, kMuchLater);
  EXPECT_EQ(5U, cache.size());

  // Ensure the entries were added.
  EXPECT_THAT(cache.Get("test1", kNow), Pointee(StrEq("foo1")));
  EXPECT_THAT(cache.Get("test2", kNow), Pointee(StrEq("foo2")));
  EXPECT_THAT(cache.Get("test3", kNow), Pointee(StrEq("foo3")));
  EXPECT_THAT(cache.Get("test4", kNow), Pointee(StrEq("foo4")));
  EXPECT_THAT(cache.Get("test5", kNow), Pointee(StrEq("foo5")));

  // Add one entry at t=kLater that expires at kHeatDeath, which will expire
  // one of test1-3.
  cache.Put("test6", "foo6", kLater, kHeatDeath);
  EXPECT_THAT(cache.Get("test6", kLater), Pointee(StrEq("foo6")));
  EXPECT_EQ(3U, cache.size());

  // Now compact at kMuchLater, which should remove all but "test6".
  cache.max_entries_ = 2;
  cache.Compact(kMuchLater);

  EXPECT_EQ(1U, cache.size());
  EXPECT_THAT(cache.Get("test6", kMuchLater), Pointee(StrEq("foo6")));

  // Finally, "test6" should not be valid at the end of the universe.
  EXPECT_FALSE(cache.Get("test6", kHeatDeath));

  // Because comparison is based on equality, not strict weak ordering, we
  // should be able to add something at kHeatDeath that expires at kMuchLater.
  cache.Put("test7", "foo7", kHeatDeath, kMuchLater);
  EXPECT_EQ(1U, cache.size());
  EXPECT_THAT(cache.Get("test7", kNow), Pointee(StrEq("foo7")));
  EXPECT_THAT(cache.Get("test7", kLater), Pointee(StrEq("foo7")));
  EXPECT_THAT(cache.Get("test7", kHeatDeath), Pointee(StrEq("foo7")));
  EXPECT_FALSE(cache.Get("test7", kMuchLater));
}

}  // namespace net
