// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_scroll_timing.h"

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/events/types/scroll_input_type.h"

namespace blink {

class PerformanceScrollTimingTest : public testing::Test {
 protected:
  LocalDOMWindow* GetWindow(const V8TestingScope& scope) const {
    auto* window = LocalDOMWindow::From(scope.GetScriptState());
    CHECK(window);
    return window;
  }

  WindowPerformance* GetPerformance(const V8TestingScope& scope) const {
    auto* performance = DOMWindowPerformance::performance(*GetWindow(scope));
    CHECK(performance);
    return performance;
  }

  test::TaskEnvironment task_environment_;

  // The Performance Scroll Timing API is gated behind a runtime feature; enable
  // it for all tests in this fixture so they exercise the production path.
  ScopedScrollPerformanceTimingForTest scroll_performance_timing_{true};
};

TEST_F(PerformanceScrollTimingTest, EntryCreation) {
  V8TestingScope scope;
  auto* window = GetWindow(scope);

  const auto* entry = MakeGarbageCollected<PerformanceScrollTiming>(
      /*start_time=*/10.0, /*duration=*/20.0, /*first_frame_time=*/15.0,
      /*delta_x=*/3, /*delta_y=*/4, AtomicString("wheel"),
      /*frames_expected=*/5, /*frames_produced=*/6,
      /*checkerboard_time=*/7.0, window->document(), window,
      GetPerformance(scope)->NavigationId().web_exposed_id);

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
  ScriptState* script_state = scope.GetScriptState();
  auto* window = GetWindow(scope);

  const auto* entry = MakeGarbageCollected<PerformanceScrollTiming>(
      /*start_time=*/10.0, /*duration=*/20.0, /*first_frame_time=*/15.0,
      /*delta_x=*/3, /*delta_y=*/4, AtomicString("wheel"),
      /*frames_expected=*/5, /*frames_produced=*/6,
      /*checkerboard_time=*/7.0, window->document(), window,
      GetPerformance(scope)->NavigationId().web_exposed_id);

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

namespace {

// Test helper: invoke AddScrollTiming with a typical hardware-derived
// timestamp pair for the given input type. The target is intentionally
// nullptr so the test exercises the null-target path through CanExposeNode.
void AddScrollTimingForTest(WindowPerformance* performance,
                            ui::ScrollInputType input_type,
                            base::TimeTicks start_time,
                            base::TimeDelta duration,
                            Node* target = nullptr) {
  performance->AddScrollTiming(start_time, start_time + duration, input_type,
                               target);
}

}  // namespace

TEST_F(PerformanceScrollTimingTest, AddScrollTimingProducesEntry) {
  V8TestingScope scope;
  auto* window = LocalDOMWindow::From(scope.GetScriptState());
  ASSERT_TRUE(window);
  auto* performance = DOMWindowPerformance::performance(*window);
  ASSERT_TRUE(performance);

  const base::TimeTicks start = base::TimeTicks::Now();
  const base::TimeDelta duration = base::Milliseconds(42);
  AddScrollTimingForTest(performance, ui::ScrollInputType::kWheel, start,
                         duration);

  const auto entries =
      performance->getBufferedEntriesByType(performance_entry_names::kScroll);
  ASSERT_EQ(1u, entries.size());
  ASSERT_EQ(PerformanceEntry::EntryType::kScroll, entries[0]->EntryTypeEnum());
  const auto* entry = static_cast<PerformanceScrollTiming*>(entries[0].Get());
  EXPECT_EQ(performance_entry_names::kScroll, entry->entryType());
  EXPECT_EQ(performance_entry_names::kScroll, entry->name());
  // startTime is the hardware timestamp converted via the time-origin.
  EXPECT_EQ(performance->MonotonicTimeToDOMHighResTimeStamp(start),
            entry->startTime());
  // duration matches end_time - start_time after the same conversion.
  EXPECT_EQ(performance->MonotonicTimeToDOMHighResTimeStamp(start + duration) -
                performance->MonotonicTimeToDOMHighResTimeStamp(start),
            entry->duration());
  EXPECT_EQ(AtomicString("wheel"), entry->scrollSource());
  // TODO(crbug.com/504094429): replace these placeholder expectations with the
  // real metric values once subsequent CLs in the incremental landing plan add
  // firstFrameTime, deltaX/Y, frame counts, and checkerboardTime.
  EXPECT_EQ(0.0, entry->firstFrameTime());
  EXPECT_EQ(0, entry->deltaX());
  EXPECT_EQ(0, entry->deltaY());
  EXPECT_EQ(0u, entry->framesExpected());
  EXPECT_EQ(0u, entry->framesProduced());
  EXPECT_EQ(0.0, entry->checkerboardTime());
  EXPECT_EQ(nullptr, entry->target());
}

TEST_F(PerformanceScrollTimingTest, AddScrollTimingMapsTouchScrollSource) {
  V8TestingScope scope;
  auto* window = LocalDOMWindow::From(scope.GetScriptState());
  ASSERT_TRUE(window);
  auto* performance = DOMWindowPerformance::performance(*window);
  ASSERT_TRUE(performance);

  AddScrollTimingForTest(performance, ui::ScrollInputType::kTouchscreen,
                         base::TimeTicks::Now(), base::Milliseconds(10));

  const auto entries =
      performance->getBufferedEntriesByType(performance_entry_names::kScroll);
  ASSERT_EQ(1u, entries.size());
  ASSERT_EQ(PerformanceEntry::EntryType::kScroll, entries[0]->EntryTypeEnum());
  EXPECT_EQ(
      AtomicString("touch"),
      static_cast<PerformanceScrollTiming*>(entries[0].Get())->scrollSource());
}

TEST_F(PerformanceScrollTimingTest, AddScrollTimingSkipsUnsupportedInputTypes) {
  V8TestingScope scope;
  auto* window = LocalDOMWindow::From(scope.GetScriptState());
  ASSERT_TRUE(window);
  auto* performance = DOMWindowPerformance::performance(*window);
  ASSERT_TRUE(performance);

  for (auto input_type :
       {ui::ScrollInputType::kAutoscroll, ui::ScrollInputType::kScrollbar}) {
    AddScrollTimingForTest(performance, input_type, base::TimeTicks::Now(),
                           base::Milliseconds(5));
  }
  EXPECT_TRUE(
      performance->getBufferedEntriesByType(performance_entry_names::kScroll)
          .empty());
}

TEST_F(PerformanceScrollTimingTest, AddScrollTimingDropsDisconnectedTarget) {
  V8TestingScope scope;
  auto* window = LocalDOMWindow::From(scope.GetScriptState());
  ASSERT_TRUE(window);
  auto* performance = DOMWindowPerformance::performance(*window);
  ASSERT_TRUE(performance);

  // A non-null but disconnected element fails `Performance::CanExposeNode`, so
  // the stored entry's target should be filtered to nullptr at write time.
  auto* disconnected =
      window->document()->CreateRawElement(html_names::kDivTag);
  ASSERT_TRUE(disconnected);
  ASSERT_FALSE(disconnected->isConnected());

  AddScrollTimingForTest(performance, ui::ScrollInputType::kWheel,
                         base::TimeTicks::Now(), base::Milliseconds(10),
                         disconnected);

  const auto entries =
      performance->getBufferedEntriesByType(performance_entry_names::kScroll);
  ASSERT_EQ(1u, entries.size());
  // Entry is produced (scroll timing is still useful without a target), but
  // the unexposable node is dropped.
  EXPECT_EQ(nullptr,
            static_cast<PerformanceScrollTiming*>(entries[0].Get())->target());
}

}  // namespace blink
