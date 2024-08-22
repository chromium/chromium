// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"

#include "base/test/trace_event_analyzer.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
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
    // Ensure we do not carry over an instance from one test to another.
    InvalidationSetToSelectorMap::StartOrStopTrackingIfNeeded(
        GetDocument().GetStyleEngine());
    CHECK(GetInstance() == nullptr);

    PageTestBase::TearDown();
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

TEST_F(InvalidationSetToSelectorMapTest, InvalidationSetRemoval) {
  StartTracing();
  SetBodyInnerHTML(R"HTML(<div id=d>D</div>)HTML");
  EXPECT_NE(GetInstance(), nullptr);

  StyleRule* style_rule = To<StyleRule>(
      css_test_helpers::ParseRule(GetDocument(), ".a .b { color: red; }"));
  AtomicString class_name("b");

  using SelectorFeatureType = InvalidationSetToSelectorMap::SelectorFeatureType;
  using IndexedSelector = InvalidationSetToSelectorMap::IndexedSelector;
  using IndexedSelectorList = InvalidationSetToSelectorMap::IndexedSelectorList;

  InvalidationSetToSelectorMap::BeginSelector(style_rule, 0);
  InvalidationSet* invalidation_set =
      DescendantInvalidationSet::Create().release();
  InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
      invalidation_set, SelectorFeatureType::kClass, class_name);
  InvalidationSetToSelectorMap::EndSelector();

  const IndexedSelectorList* result = InvalidationSetToSelectorMap::Lookup(
      invalidation_set, SelectorFeatureType::kClass, class_name);
  EXPECT_TRUE(
      result->Contains(MakeGarbageCollected<IndexedSelector>(style_rule, 0)));

  // Release the invalidation set but retain the pointer so we can confirm that
  // looking it up no longer returns any results.
  EXPECT_TRUE(invalidation_set->HasOneRef());
  invalidation_set->Release();

  result = InvalidationSetToSelectorMap::Lookup(
      invalidation_set, SelectorFeatureType::kClass, class_name);
  EXPECT_EQ(result, nullptr);

  StopTracing();
}

TEST_F(InvalidationSetToSelectorMapTest, StartTracingLate) {
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

  StartTracing();

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

TEST_F(InvalidationSetToSelectorMapTest, StartTracingLateWithNestedRules) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @media screen {
        @supports (color: green) {
          .a .x { color: red; }
          .b .x { color: green; }
          .c .x { color: blue; }
        }
      }
    </style>
    <div id=parent class=a>Parent
      <div class=x>Child</div>
    </div>
  )HTML");

  StartTracing();

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

TEST_F(InvalidationSetToSelectorMapTest,
       StartTracingLateWithSiblingAndDescendantRules) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .a ~ .b { color: red; }
      .a .c { color: green; }
    </style>
    <div id=parent class=a>Parent
      <div class=c>Child</div>
    </div>
  )HTML");

  StartTracing();

  GetElementById("parent")->removeAttribute(html_names::kClassAttr);
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
        EXPECT_EQ((*selector_list)[0], ".a .c");
        found_event_count++;
      }
    }
  }
  EXPECT_EQ(found_event_count, 1u);
}

TEST_F(InvalidationSetToSelectorMapTest,
       StartTracingLateWithPendingInsertRule) {
  SetBodyInnerHTML(R"HTML(
    <style id=target>
      .a .b { color: red; }
    </style>
    <div id=parent class=c>Parent
      <div class=d>Child</div>
    </div>
  )HTML");

  StartTracing();

  DummyExceptionStateForTesting exception_state;
  CSSStyleSheet* sheet =
      To<HTMLStyleElement>(GetElementById("target"))->sheet();
  sheet->insertRule(".c .d { color: green; }", 0, exception_state);
  UpdateAllLifecyclePhasesForTest();
  GetElementById("parent")->removeAttribute(html_names::kClassAttr);
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
        EXPECT_EQ((*selector_list)[0], ".c .d");
        found_event_count++;
      }
    }
  }
  EXPECT_EQ(found_event_count, 1u);
}

TEST_F(InvalidationSetToSelectorMapTest, HandleRebuildAfterRuleSetChange) {
  // This test is intended to cover the case that necessitates us walking both
  // global and per-sheet rule sets when revisiting invalidation data on a late
  // attach.
  //
  // The rule we're trying to catch is the `.a .b` rule. The `.a .c` rule is
  // also important, and the fact that it is in a separate sheet is important.
  // Without that separate sheet, when we build the global rule set we would
  // just AddRef the `.a {.b}` invalidation set from the per-sheet rule set.
  // When tracing starts, revisiting the global rule set will associate that
  // `.a {.b}` invalidation set with the `.a .b` selector. If the global rule
  // set needs to be rebuilt, we'd AddRef the same invalidation set again, so
  // the association would remain stable.
  //
  // By contrast, having another sheet with an invalidation set also keyed at
  // `.a` forces a copy-on-write and combine. The combined `.a {.b .c}`
  // invalidation set gets stored in the global rule set, and it's what we find
  // on a revisit. The next time we rebuild the global rule set while tracing is
  // still active, we repeat the copy-on-write and combine, which generates a
  // brand-new `.a {.b .c}` invalidation set. If we hadn't walked the per-sheet
  // rule sets, we wouldn't know that the `.a .b` selector contributed to the
  // `.a {.b}` invalidation set and would not follow that selector through the
  // combine. Then, when an invalidation happens due to the `.b` entry on the
  // new `.a {.b .c}` invalidation set, we wouldn't know what selector put that
  // entry there.
  SetBodyInnerHTML(R"HTML(
    <style>
      .a .b { color: red; }
    </style>
    <style>
      .a .c { color: green; }
    </style>
    <div id=parent class=a>Parent
      <div class=b>Child</div>
    </div>
  )HTML");

  StartTracing();

  // Invalidation data revisit happens on the first lifecycle update following
  // the start of tracing. Perform a simple mutation to cause that to happen.
  GetDocument().body()->appendChild(
      GetDocument().CreateRawElement(html_names::kDivTag));
  UpdateAllLifecyclePhasesForTest();

  // Insert a new stylesheet to cause a rebuild of the global rule set.
  InsertStyleElement("#nobody { color: blue; }");
  UpdateAllLifecyclePhasesForTest();

  // Now perform an invalidation-causing mutation and confirm we can follow the
  // invalidation back to the style rule.
  GetElementById("parent")->removeAttribute(html_names::kClassAttr);
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
        EXPECT_EQ((*selector_list)[0], ".a .b");
        found_event_count++;
      }
    }
  }
  EXPECT_EQ(found_event_count, 1u);
}

}  // namespace blink
