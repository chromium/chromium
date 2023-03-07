// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/date_math.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(DateMathTest, MakeRFC2822DateString) {
  {
    auto string = MakeRFC2822DateString(base::Time::Now(), 0);
    EXPECT_TRUE(string.has_value());
  }

#if BUILDFLAG(IS_WIN)
  {
    // On Windows, a negative date before 1601-01-01 00:00:00.000 UTC cannot be
    // exploded so MakeRFC2822DateString will fail.
    base::Time date =
        base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(-1));
    auto string = MakeRFC2822DateString(date, 0);
    EXPECT_FALSE(string.has_value());
  }
#endif  // BUIDFLAG(IS_WIN)
}

}  // namespace WTF
