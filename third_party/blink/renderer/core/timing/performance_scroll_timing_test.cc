// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_scroll_timing.h"

#include <limits>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "cc/metrics/scroll_timing_buffer.h"
#include "cc/trees/compositor_commit_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
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

class ScrollTimingWidgetIntegrationTest : public SimTest {
 protected:
  ScrollTimingWidgetIntegrationTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    SimTest::SetUp();
    SimRequest request("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    request.Complete(kPage);
    Compositor().BeginFrame();

    // SimTest's single-threaded compositor has no Scroll Timing buffer, so
    // attach one whose producer the test controls.
    producer_ = cc::ScrollTimingBuffer::Create();
    ASSERT_TRUE(producer_);
    GetWebFrameWidget()
        .LayerTreeHostForTesting()
        ->InitializeScrollTimingBufferConsumer(producer_->TakeReadOnlyRegion());
  }

  WindowPerformance* GetPerformance() {
    return DOMWindowPerformance::performance(Window());
  }

  PerformanceEntryVector Entries() {
    return GetPerformance()->getBufferedEntriesByType(
        performance_entry_names::kScroll);
  }

  Node* EntryTarget(wtf_size_t index) {
    return static_cast<PerformanceScrollTiming*>(Entries()[index].Get())
        ->target();
  }

  cc::ElementId ScrollerId(Document& document) {
    return document.getElementById(AtomicString("scroller"))
        ->GetLayoutBox()
        ->GetScrollableArea()
        ->GetScrollElementId();
  }
  cc::ElementId ScrollerId() { return ScrollerId(GetDocument()); }

  cc::ElementId ViewportId() {
    return WebView().GetPage()->GetVisualViewport().GetScrollElementId();
  }

  void Publish(cc::ElementId id,
               base::TimeTicks start_time = base::TimeTicks::Now()) {
    producer_->Push({
        .start_time = start_time,
        .end_time = start_time + base::Milliseconds(10),
        .input_type = ui::ScrollInputType::kWheel,
        .element_id = id,
    });
  }

  void NotifyScrollEnd() {
    cc::CompositorCommitData data;
    data.scroll_end_data.done_containers.insert(ScrollerId());
    GetWebFrameWidget().UpdateCompositorScrollState(data);
  }

  double Eval(const char* script) {
    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Value> result = MainFrame().ExecuteScriptAndReturnValue(
        WebScriptSource(WebString::FromUtf8(script)));
    CHECK(!result.IsEmpty());
    return result->NumberValue(script_state->GetContext()).ToChecked();
  }

  // Starts a buffered observer that records its latest callback in
  // `delivered` and `dropped`.
  void ObserveDroppedEntries() {
    Eval(R"JS(
      var delivered = 0, dropped = -1;
      var observer = new PerformanceObserver((list, observer, options) => {
        delivered = list.getEntries().length;
        dropped = options.droppedEntriesCount;
        window.dispatchEvent(new Event('scrolltimingdelivered'));
      });
      observer.observe({type: 'scroll', buffered: true});
      0;
    )JS");
    MakeGarbageCollected<WaitForEvent>(&Window(),
                                       AtomicString("scrolltimingdelivered"));
  }

  static constexpr char kPage[] = R"HTML(
    <!doctype html>
    <div id="scroller" style="overflow:scroll;width:100px;height:100px">
      <div style="height:1000px"></div>
    </div>
  )HTML";

  ScopedScrollPerformanceTimingForTest scroll_performance_timing_{true};
  std::unique_ptr<cc::ScrollTimingBuffer> producer_;
};

TEST_F(ScrollTimingWidgetIntegrationTest, ObserveImportsBeforeRegistration) {
  Publish(ScrollerId());
  EXPECT_EQ(0, Eval(R"JS(
    var a = new PerformanceObserver(() => {});
    a.observe({type: 'scroll'});
    a.takeRecords().length;
  )JS"));
  Publish(ScrollerId());
  EXPECT_EQ(2, Eval(R"JS(
    var b = new PerformanceObserver(() => {});
    b.observe({type: 'scroll', buffered: true});
    b.takeRecords().length;
  )JS"));
  EXPECT_EQ(1, Eval("a.takeRecords().length"));
}

TEST_F(ScrollTimingWidgetIntegrationTest, FilterChangeImportsForObservers) {
  Eval(R"JS(
    var a = new PerformanceObserver(() => {});
    a.observe({entryTypes: ['scroll']});
    0;
  )JS");
  Publish(ScrollerId());
  Eval("a.observe({entryTypes: ['mark']}); 0");
  EXPECT_EQ(1, Eval("a.takeRecords().length"));
}

TEST_F(ScrollTimingWidgetIntegrationTest, HistoryCountsOnlyBlinkDrops) {
  constexpr uint32_t kCapacity = cc::ScrollTimingBuffer::kCapacity;
  constexpr uint32_t kOverwritten = 10;
  for (uint32_t i = 0; i < kCapacity + kOverwritten; ++i) {
    Publish(ScrollerId());
  }
  ObserveDroppedEntries();
  const auto history = Entries();
  EXPECT_EQ(kCapacity, Eval("delivered"));
  EXPECT_EQ(0, Eval("dropped"));

  // A full history still delivers live entries and counts the history drop.
  Eval("dropped = -1; observer.observe({type: 'scroll'}); 0");
  Publish(ScrollerId());
  NotifyScrollEnd();
  MakeGarbageCollected<WaitForEvent>(&Window(),
                                     AtomicString("scrolltimingdelivered"));
  EXPECT_EQ(1, Eval("delivered"));
  EXPECT_EQ(1, Eval("dropped"));
  EXPECT_EQ(history, Entries());
}

TEST_F(ScrollTimingWidgetIntegrationTest, ScrollEndReadsAndRetries) {
  Publish(cc::ElementId(std::numeric_limits<uint64_t>::max() - 1));
  Publish(ScrollerId());
  NotifyScrollEnd();
  ASSERT_EQ(1u, Entries().size());
  EXPECT_EQ(GetElementById("scroller"), EntryTarget(0));

  task_environment().FastForwardBy(base::Milliseconds(50));
  // A later scroll end does not postpone the pending retry.
  NotifyScrollEnd();
  Publish(ViewportId());
  task_environment().FastForwardBy(base::Milliseconds(49));
  EXPECT_EQ(1u, Entries().size());
  task_environment().FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(2u, Entries().size());
  EXPECT_EQ(&GetDocument(), EntryTarget(1));

  NotifyScrollEnd();
  Publish(ScrollerId());
  GetWebFrameWidget().widget_base_for_testing()->WasHidden();
  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_EQ(2u, Entries().size());
}

TEST_F(ScrollTimingWidgetIntegrationTest, NavigationDiscardsEarlierRecords) {
  // The viewport ID persists across navigation.
  for (uint32_t i = 0; i <= cc::ScrollTimingBuffer::kCapacity; ++i) {
    Publish(ViewportId());
  }
  const base::TimeTicks before_navigation = base::TimeTicks::Now();
  task_environment().AdvanceClock(base::Milliseconds(1));

  SimRequest request("https://example.com/next", "text/html");
  LoadURL("https://example.com/next");
  request.Complete(kPage);
  Compositor().BeginFrame();

  // A gesture that started before the navigation is rejected even if it is
  // published afterwards.
  Publish(ViewportId(), before_navigation);
  Publish(ViewportId());
  ObserveDroppedEntries();
  EXPECT_EQ(1, Eval("delivered"));
  EXPECT_EQ(0, Eval("dropped"));
  ASSERT_EQ(1u, Entries().size());
  EXPECT_EQ(&GetDocument(), EntryTarget(0));
}

class ScrollTimingChildFrameTest
    : public ScrollTimingWidgetIntegrationTest,
      public testing::WithParamInterface<const char*> {};

TEST_P(ScrollTimingChildFrameTest, ChildFrameTargetsDoNotReachLocalRoot) {
  SimRequest child_request(GetParam(), "text/html");
  Eval(Format(R"JS(
    var child = document.createElement('iframe');
    child.src = '{}';
    document.body.append(child);
    0;
  )JS",
              GetParam())
           .Utf8()
           .c_str());
  child_request.Complete(kPage);
  Compositor().BeginFrame();
  Document* child_document =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
          ->GetDocument();

  Publish(ScrollerId(*child_document));
  EXPECT_EQ(0, Eval(R"JS(
    var observer = new PerformanceObserver(() => {});
    observer.observe({type: 'scroll', buffered: true});
    observer.takeRecords().length;
  )JS"));
  EXPECT_TRUE(Entries().empty());

  Publish(ScrollerId(*child_document));
  Publish(ScrollerId());
  NotifyScrollEnd();
  ASSERT_EQ(1u, Entries().size());
  EXPECT_EQ(GetElementById("scroller"), EntryTarget(0));
  EXPECT_EQ(1, Eval("observer.takeRecords().length"));
  EXPECT_TRUE(DOMWindowPerformance::performance(*child_document->domWindow())
                  ->getBufferedEntriesByType(performance_entry_names::kScroll)
                  .empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollTimingChildFrameTest,
                         testing::Values("https://example.com/child",
                                         "https://sub.example.com/child",
                                         "https://other.test/child"));

}  // namespace blink
