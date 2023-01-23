// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_entry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

class PerformanceEntryTest : public testing::Test {};
TEST(PerformanceEntryTest, GetNavigationId) {
  V8TestingScope scope;

  EXPECT_EQ(1u, PerformanceEntry::GetNavigationId(scope.GetScriptState()));

  scope.GetFrame().DomWindow()->IncrementNavigationId();
  EXPECT_EQ(2u, PerformanceEntry::GetNavigationId(scope.GetScriptState()));
}
}  // namespace blink
