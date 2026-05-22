// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_scroll_timing.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class PerformanceScrollTimingTest : public testing::Test {
 protected:
  LocalDOMWindow* GetWindow(const V8TestingScope& scope) const {
    auto* const window = LocalDOMWindow::From(scope.GetScriptState());
    CHECK(window);
    return window;
  }

  WindowPerformance* GetPerformance(const V8TestingScope& scope) const {
    auto* const performance =
        DOMWindowPerformance::performance(*GetWindow(scope));
    CHECK(performance);
    return performance;
  }

  test::TaskEnvironment task_environment_;
};

TEST_F(PerformanceScrollTimingTest, EntryCreation) {
  V8TestingScope scope;
  auto* const window = GetWindow(scope);

  const auto* const entry = MakeGarbageCollected<PerformanceScrollTiming>(
      /*start_time=*/10.0, /*duration=*/20.0, /*first_frame_time=*/15.0,
      /*delta_x=*/3, /*delta_y=*/4, AtomicString("wheel"),
      /*frames_expected=*/5, /*frames_produced=*/6,
      /*checkerboard_time=*/7.0, window->document(), window,
      GetPerformance(scope)->NavigationId());

  EXPECT_EQ(AtomicString("scroll"), entry->name());
  EXPECT_EQ(AtomicString("scroll"), entry->entryType());
  EXPECT_EQ(PerformanceEntry::EntryType::kScroll, entry->EntryTypeEnum());
  EXPECT_EQ(
      PerformanceEntry::EntryType::kScroll,
      PerformanceEntry::ToEntryTypeEnum(performance_entry_names::kScroll));
  EXPECT_EQ(10.0, entry->startTime());
  EXPECT_EQ(20.0, entry->duration());
  EXPECT_EQ(15.0, entry->firstFrameTime());
  EXPECT_EQ(3, entry->deltaX());
  EXPECT_EQ(4, entry->deltaY());
  EXPECT_EQ(AtomicString("wheel"), entry->scrollSource());
  EXPECT_EQ(5u, entry->framesExpected());
  EXPECT_EQ(6u, entry->framesProduced());
  EXPECT_EQ(7.0, entry->checkerboardTime());
  EXPECT_EQ(window->document(), entry->target());
}

TEST_F(PerformanceScrollTimingTest, ToJSON) {
  V8TestingScope scope;
  ScriptState* const script_state = scope.GetScriptState();
  auto* const window = GetWindow(scope);

  const auto* const entry = MakeGarbageCollected<PerformanceScrollTiming>(
      /*start_time=*/10.0, /*duration=*/20.0, /*first_frame_time=*/15.0,
      /*delta_x=*/3, /*delta_y=*/4, AtomicString("wheel"),
      /*frames_expected=*/5, /*frames_produced=*/6,
      /*checkerboard_time=*/7.0, window->document(), window,
      GetPerformance(scope)->NavigationId());

  const ScriptValue json_object = entry->toJSONForBinding(script_state);
  EXPECT_TRUE(json_object.IsObject());

  const String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  const auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json_string.Utf8(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(parsed_json->is_dict());
  const auto& json_dict = parsed_json->GetDict();

  EXPECT_EQ("scroll", *json_dict.FindString("name"));
  EXPECT_EQ("scroll", *json_dict.FindString("entryType"));
  EXPECT_EQ(10.0, json_dict.FindDouble("startTime").value());
  EXPECT_EQ(20.0, json_dict.FindDouble("duration").value());
  EXPECT_EQ(15.0, json_dict.FindDouble("firstFrameTime").value());
  EXPECT_EQ(3, json_dict.FindInt("deltaX").value());
  EXPECT_EQ(4, json_dict.FindInt("deltaY").value());
  EXPECT_EQ("wheel", *json_dict.FindString("scrollSource"));
  EXPECT_EQ(5.0, json_dict.FindDouble("framesExpected").value());
  EXPECT_EQ(6.0, json_dict.FindDouble("framesProduced").value());
  EXPECT_EQ(7.0, json_dict.FindDouble("checkerboardTime").value());
}

TEST_F(PerformanceScrollTimingTest, PerformanceObserverSupportedEntryTypes) {
  ScopedScrollPerformanceTimingForTest scroll_performance_timing(true);
  V8TestingScope scope;

  const Vector<AtomicString> supported_entry_types =
      PerformanceObserver::supportedEntryTypes(scope.GetScriptState());

  EXPECT_TRUE(supported_entry_types.Contains(performance_entry_names::kScroll));
}

// Sanity check that `getBufferedEntriesByType("scroll")` is a recognized,
// non-crashing query on a fresh Performance object and returns an empty list.
TEST_F(PerformanceScrollTimingTest, GetBufferedEntriesByTypeWhenEmpty) {
  V8TestingScope scope;

  EXPECT_TRUE(GetPerformance(scope)
                  ->getBufferedEntriesByType(performance_entry_names::kScroll)
                  .empty());
}

}  // namespace blink
