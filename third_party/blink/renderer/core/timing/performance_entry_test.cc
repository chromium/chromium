// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_entry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class PerformanceEntryTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(PerformanceEntryTest, GetNavigationId) {
  V8TestingScope scope;

  String navigation_id1 =
      PerformanceEntry::GetNavigationId(scope.GetScriptState());

  scope.GetFrame().DomWindow()->GenerateNewNavigationId();
  String navigation_id2 =
      PerformanceEntry::GetNavigationId(scope.GetScriptState());

  EXPECT_NE(navigation_id1, navigation_id2);
}
}  // namespace blink
