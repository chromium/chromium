// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/assertions.h"

#include "base/notreached.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace WTF {

TEST(AssertionsTest, Assertions) {
  DCHECK(true);
#if DCHECK_IS_ON()
  EXPECT_DEATH_IF_SUPPORTED(DCHECK(false), "");
  EXPECT_DEATH_IF_SUPPORTED(DCHECK_AT(false, __FILE__, __LINE__), "");
#else
  DCHECK(false);
  DCHECK_AT(false, __FILE__, __LINE__);
#endif

  CHECK(true);
  EXPECT_DEATH_IF_SUPPORTED(CHECK(false), "");

  EXPECT_DEATH_IF_SUPPORTED(NOTREACHED(), "");

  SECURITY_DCHECK(true);
#if ENABLE_SECURITY_ASSERT
  EXPECT_DEATH_IF_SUPPORTED(SECURITY_DCHECK(false), "");
#else
  SECURITY_DCHECK(false);
#endif

  SECURITY_CHECK(true);
  EXPECT_DEATH_IF_SUPPORTED(SECURITY_CHECK(false), "");
}

}  // namespace WTF
