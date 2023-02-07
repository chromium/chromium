// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(AttributionUtilsTest, LastTriggerTimeForReportTime) {
  const base::Time time = base::Time::Now();
  EXPECT_EQ(LastTriggerTimeForReportTime(time), time - base::Hours(1));
}

}  // namespace
}  // namespace content
