// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_entry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

class PerformanceEntryTest : public testing::Test {};
TEST(PerformanceEntryTest, GetNavigationCounter) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  EXPECT_EQ(1u, PerformanceEntry::GetNavigationId(script_state));

  scope.GetFrame().IncrementNavigationId();
  EXPECT_EQ(2u, PerformanceEntry::GetNavigationId(script_state));

  scope.GetFrame().IncrementNavigationId();
  EXPECT_EQ(3u, PerformanceEntry::GetNavigationId(script_state));
}
}  // namespace blink
