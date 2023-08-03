// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note that while this file is in testing/ and tests GTest macros, it is built
// as part of Chromium's unit_tests target because the project does not build
// or run GTest's internal test suite.

#import "testing/gtest_mac.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/googletest/src/googletest/include/gtest/internal/gtest-port.h"

namespace testing {
namespace internal {
// This function is tested within this file, but it's not part of the public
// API, and since it's a free function there's no way to friend the test for it.
extern std::string StringDescription(id<NSObject> obj);
}
}

TEST(GTestMac, NSStringComparators) {
  // This test wants to really guarantee that s1 and s2 aren't the same address,
  // so it constructs a string this way. In theory this could be done via
  // [NSString stringWithString:] but that causes an error about using a
  // redundant literal :)
  NSString* s1 = [NSString stringWithFormat:@"%@", @"a"];
  NSString* s2 = @"a";

  EXPECT_NSEQ(@"a", @"a");
  EXPECT_NE(s1, s2);
  EXPECT_NSEQ(s1, s2);
  ASSERT_NE(s1, s2);
  ASSERT_NSEQ(s1, s2);

  ASSERT_NSNE(@"a", @"b");

  EXPECT_NSEQ(nil, nil);
  EXPECT_NSNE(nil, @"a");
  EXPECT_NSNE(@"a", nil);
}

TEST(GTestMac, NSNumberComparators) {
  EXPECT_NSNE(@2, @42);
  EXPECT_NSEQ(@42, @42);
}

#if !defined(GTEST_OS_IOS)

TEST(GTestMac, NSRectComparators) {
  ASSERT_NSEQ(NSMakeRect(1, 2, 3, 4), NSMakeRect(1, 2, 3, 4));
  ASSERT_NSNE(NSMakeRect(1, 2, 3, 4), NSMakeRect(5, 6, 7, 8));

  EXPECT_NSEQ(NSMakeRect(1, 2, 3, 4), NSMakeRect(1, 2, 3, 4));
  EXPECT_NSNE(NSMakeRect(1, 2, 3, 4), NSMakeRect(5, 6, 7, 8));
}

TEST(GTestMac, NSPointComparators) {
  ASSERT_NSEQ(NSMakePoint(1, 2), NSMakePoint(1, 2));
  ASSERT_NSNE(NSMakePoint(1, 2), NSMakePoint(1, 3));
  ASSERT_NSNE(NSMakePoint(1, 2), NSMakePoint(2, 2));

  EXPECT_NSEQ(NSMakePoint(3, 4), NSMakePoint(3, 4));
  EXPECT_NSNE(NSMakePoint(3, 4), NSMakePoint(3, 3));
  EXPECT_NSNE(NSMakePoint(3, 4), NSMakePoint(4, 3));
}

TEST(GTestMac, NSRangeComparators) {
  ASSERT_NSEQ(NSMakeRange(1, 2), NSMakeRange(1, 2));
  ASSERT_NSNE(NSMakeRange(1, 2), NSMakeRange(1, 3));
  ASSERT_NSNE(NSMakeRange(1, 2), NSMakeRange(2, 2));

  EXPECT_NSEQ(NSMakeRange(3, 4), NSMakeRange(3, 4));
  EXPECT_NSNE(NSMakeRange(3, 4), NSMakeRange(3, 3));
  EXPECT_NSNE(NSMakeRange(3, 4), NSMakeRange(4, 4));
}

TEST(GTestMac, StringDescription) {
  using testing::internal::StringDescription;
  EXPECT_EQ(StringDescription(@4), "4");
  EXPECT_EQ(StringDescription(@"foo"), "foo");
  EXPECT_EQ(StringDescription(nil), "(null)");
}

#endif  // !GTEST_OS_IOS
