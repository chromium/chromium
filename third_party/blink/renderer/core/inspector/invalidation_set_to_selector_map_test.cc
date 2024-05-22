// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"

#include "base/test/trace_event_analyzer.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class InvalidationSetToSelectorMapTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    CHECK(GetInstance() == nullptr);
  }
  void TearDown() override {
    PageTestBase::TearDown();

    // Ensure we do not carry over an instance from one test to another.
    InvalidationSetToSelectorMap::StartOrStopTrackingIfNeeded();
    CHECK(GetInstance() == nullptr);
  }

  void StartTracing() {
    trace_analyzer::Start(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
  }
  void StartTracingWithoutInvalidationTracking() {
    trace_analyzer::Start(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"));
  }
  std::unique_ptr<trace_analyzer::TraceAnalyzer> StopTracing() {
    return trace_analyzer::Stop();
  }
  InvalidationSetToSelectorMap* GetInstance() {
    return InvalidationSetToSelectorMap::GetInstanceReference().Get();
  }
};

TEST_F(InvalidationSetToSelectorMapTest, TrackerLifetime) {
  ASSERT_EQ(GetInstance(), nullptr);

  StartTracing();
  SetBodyInnerHTML(R"HTML(<div id=d>D</div>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(GetInstance(), nullptr);
  GetElementById("d")->setAttribute(html_names::kStyleAttr,
                                    AtomicString("color: red"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(GetInstance(), nullptr);

  StopTracing();
  GetElementById("d")->setAttribute(html_names::kStyleAttr,
                                    AtomicString("color: green"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetInstance(), nullptr);

  StartTracingWithoutInvalidationTracking();
  GetElementById("d")->setAttribute(html_names::kStyleAttr,
                                    AtomicString("color: blue"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetInstance(), nullptr);
  StopTracing();
}

TEST_F(InvalidationSetToSelectorMapTest, ClassMatch) {
  StartTracing();
  SetBodyInnerHTML(R"HTML(
    <style>
      .a .x { color: red; }
      .b .x { color: green; }
      .c .x { color: blue; }
    </style>
    <div id=parent class=a>Parent
      <div class=x>Child</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  GetElementById("parent")->setAttribute(html_names::kClassAttr,
                                         AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();

  auto analyzer = StopTracing();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(trace_analyzer::Query::EventNameIs(
                           "StyleInvalidatorInvalidationTracking"),
                       &events);
  size_t found_event_count = 0;
  for (auto event : events) {
    ASSERT_TRUE(event->HasDictArg("data"));
    base::Value::Dict data_dict = event->GetKnownArgAsDict("data");
    std::string* reason = data_dict.FindString("reason");
    if (reason != nullptr && *reason == "Invalidation set matched class") {
      base::Value::List* selector_list = data_dict.FindList("selectors");
      if (selector_list != nullptr) {
        EXPECT_EQ(selector_list->size(), 1u);
        EXPECT_EQ((*selector_list)[0], ".b .x");
        found_event_count++;
      }
    }
  }
  EXPECT_EQ(found_event_count, 1u);
}

TEST_F(InvalidationSetToSelectorMapTest, ClassMatchWithMultipleInvalidations) {
  StartTracing();
  SetBodyInnerHTML(R"HTML(
    <style>
      .a .x { color: red; }
      .b .x { color: green; }
      .c .x { color: blue; }
    </style>
    <div id=parent class=a>Parent
      <div class=x>Child</div>
      <div class=x>Child</div>
      <div class=x>Child</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  GetElementById("parent")->setAttribute(html_names::kClassAttr,
                                         AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();

  auto analyzer = StopTracing();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(trace_analyzer::Query::EventNameIs(
                           "StyleInvalidatorInvalidationTracking"),
                       &events);
  size_t found_event_count = 0;
  for (auto event : events) {
    ASSERT_TRUE(event->HasDictArg("data"));
    base::Value::Dict data_dict = event->GetKnownArgAsDict("data");
    std::string* reason = data_dict.FindString("reason");
    if (reason != nullptr && *reason == "Invalidation set matched class") {
      base::Value::List* selector_list = data_dict.FindList("selectors");
      if (selector_list != nullptr) {
        EXPECT_EQ(selector_list->size(), 1u);
        EXPECT_EQ((*selector_list)[0], ".b .x");
        found_event_count++;
      }
    }
  }
  EXPECT_EQ(found_event_count, 3u);
}

TEST_F(InvalidationSetToSelectorMapTest, ClassMatchWithCombine) {
  StartTracing();
  SetBodyInnerHTML(R"HTML(
    <style>
      .a .x { color: red; }
      .b .x { color: green; }
      .c .x { color: blue; }
    </style>
    <style>
      .b .w .x { color: black; }
    </style>
    <div id=parent class=a>Parent
      <div class=x>Child</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  GetElementById("parent")->setAttribute(html_names::kClassAttr,
                                         AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();

  auto analyzer = StopTracing();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(trace_analyzer::Query::EventNameIs(
                           "StyleInvalidatorInvalidationTracking"),
                       &events);
  size_t found_event_count = 0;
  for (auto event : events) {
    ASSERT_TRUE(event->HasDictArg("data"));
    base::Value::Dict data_dict = event->GetKnownArgAsDict("data");
    std::string* reason = data_dict.FindString("reason");
    if (reason != nullptr && *reason == "Invalidation set matched class") {
      base::Value::List* selector_list = data_dict.FindList("selectors");
      if (selector_list != nullptr) {
        EXPECT_EQ(selector_list->size(), 2u);
        // The map stores selectors in a HeapHashSet; they can be output to the
        // trace event list in either order.
        if ((*selector_list)[0] == ".b .x") {
          EXPECT_EQ((*selector_list)[1], ".b .w .x");
        } else {
          EXPECT_EQ((*selector_list)[0], ".b .w .x");
          EXPECT_EQ((*selector_list)[1], ".b .x");
        }
        found_event_count++;
      }
    }
  }
  EXPECT_EQ(found_event_count, 1u);
}

TEST_F(InvalidationSetToSelectorMapTest, SelfInvalidation) {
  StartTracing();
  SetBodyInnerHTML(R"HTML(
    <style>
      .a { color: red; }
      .b { color: green; }
      .c { color: blue; }
    </style>
    <div id=parent class=a>Parent
      <div class=x>Child</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  GetElementById("parent")->setAttribute(html_names::kClassAttr,
                                         AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();

  auto analyzer = StopTracing();
  trace_analyzer::TraceEventVector events;

  analyzer->FindEvents(
      trace_analyzer::Query::EventNameIs("ScheduleStyleInvalidationTracking") ||
          trace_analyzer::Query::EventNameIs(
              "StyleInvalidatorInvalidationTracking"),
      &events);
  ASSERT_EQ(events.size(), 4u);
  EXPECT_EQ(events[0]->name, "ScheduleStyleInvalidationTracking");
  EXPECT_EQ(*(events[0]->GetKnownArgAsDict("data").FindString(
                "invalidatedSelectorId")),
            "class");
  EXPECT_EQ(*(events[0]->GetKnownArgAsDict("data").FindString("changedClass")),
            "b");
  EXPECT_EQ(events[1]->name, "ScheduleStyleInvalidationTracking");
  EXPECT_EQ(*(events[1]->GetKnownArgAsDict("data").FindString(
                "invalidatedSelectorId")),
            "class");
  EXPECT_EQ(*(events[1]->GetKnownArgAsDict("data").FindString("changedClass")),
            "a");
  // Because self invalidations are largely handled via the Bloom filter and/or
  // the singleton SelfInvalidationSet, we don't expect selectors. But the
  // preceding schedule events do give us context for what changed.
  EXPECT_EQ(events[2]->name, "StyleInvalidatorInvalidationTracking");
  EXPECT_EQ(*(events[2]->GetKnownArgAsDict("data").FindString("reason")),
            "Invalidation set invalidates self");
  EXPECT_EQ(events[3]->name, "StyleInvalidatorInvalidationTracking");
  EXPECT_EQ(*(events[3]->GetKnownArgAsDict("data").FindString("reason")),
            "Invalidation set invalidates self");
}

TEST_F(InvalidationSetToSelectorMapTest, SubtreeInvalidation) {
  StartTracing();
  SetBodyInnerHTML(R"HTML(
    <style>
      .a * { color: red; }
      .b * { color: green; }
      .c * { color: blue; }
    </style>
    <div id=parent class=a>Parent
      <div class=x>Child</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  GetElementById("parent")->setAttribute(html_names::kClassAttr,
                                         AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();

  auto analyzer = StopTracing();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(trace_analyzer::Query::EventNameIs(
                           "StyleInvalidatorInvalidationTracking"),
                       &events);
  size_t found_event_count = 0;
  for (auto event : events) {
    ASSERT_TRUE(event->HasDictArg("data"));
    base::Value::Dict data_dict = event->GetKnownArgAsDict("data");
    std::string* reason = data_dict.FindString("reason");
    if (reason != nullptr &&
        *reason == "Invalidation set invalidates subtree") {
      base::Value::List* selector_list = data_dict.FindList("selectors");
      if (selector_list != nullptr) {
        EXPECT_EQ(selector_list->size(), 1u);
        EXPECT_EQ((*selector_list)[0], ".b *");
        found_event_count++;
      }
    }
  }
  EXPECT_EQ(found_event_count, 1u);
}

}  // namespace blink
