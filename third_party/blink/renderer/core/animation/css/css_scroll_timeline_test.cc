// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"

#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSScrollTimelineTest : public PageTestBase,
                              private ScopedCSSScrollTimelineForTest {
 public:
  CSSScrollTimelineTest() : ScopedCSSScrollTimelineForTest(true) {}

  bool HasObservers(const AtomicString& id) {
    return GetDocument().GetIdTargetObserverRegistry().HasObservers(id);
  }
};

TEST_F(CSSScrollTimelineTest, IdObserverElementRemoval) {
  ASSERT_FALSE(HasObservers("scroller"));

  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { width: 100px; }
        to { width: 200px; }
      }
      @scroll-timeline timeline {
        source: selector(#scroller);
        time-range: 10s;
      }
      div {
        animation: anim 10s;
        animation-timeline: timeline;
      }
    </style>
    <div id=element1></div>
    <div id=element2></div>
  )HTML");

  EXPECT_TRUE(HasObservers("scroller"));

  Element* element1 = GetDocument().getElementById("element1");
  Element* element2 = GetDocument().getElementById("element2");
  ASSERT_TRUE(element1);
  ASSERT_TRUE(element2);

  element1->remove();
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(HasObservers("scroller"));

  element2->remove();
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(HasObservers("scroller"));
}

TEST_F(CSSScrollTimelineTest, IdObserverRuleInsertion) {
  ASSERT_FALSE(HasObservers("scroller1"));
  ASSERT_FALSE(HasObservers("scroller2"));
  ASSERT_FALSE(HasObservers("scroller3"));
  ASSERT_FALSE(HasObservers("redefined"));
  ASSERT_FALSE(HasObservers("offset1"));
  ASSERT_FALSE(HasObservers("offset2"));

  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { width: 100px; }
        to { width: 200px; }
      }
      @scroll-timeline timeline1 {
        source: selector(#scroller1);
        time-range: 10s;
      }
      @scroll-timeline timeline2 {
        source: selector(#scroller2);
        time-range: 10s;
        start: selector(#offset1);
      }
      div {
        animation: anim 10s;
      }
      #element1 {
        animation-timeline: timeline1;
      }
      #element2 {
        animation-timeline: timeline2;
      }
    </style>
    <div id=element1></div>
    <div id=element2></div>
    <div id=element3></div>
  )HTML");

  EXPECT_TRUE(HasObservers("scroller1"));
  EXPECT_TRUE(HasObservers("scroller2"));
  EXPECT_TRUE(HasObservers("offset1"));

  Element* element1 = GetDocument().getElementById("element1");
  Element* element2 = GetDocument().getElementById("element2");
  ASSERT_TRUE(element1);
  ASSERT_TRUE(element2);

  // Insert a <style> element which redefines timeline2, and also
  // creates an additional timeline (timeline3).
  auto* style_element = MakeGarbageCollected<HTMLStyleElement>(
      GetDocument(), CreateElementFlags());
  style_element->setTextContent(R"CSS(
      @scroll-timeline timeline2 {
        source: selector(#redefined);
        time-range: 10s;
        start: selector(#offset2);
      }
      @scroll-timeline timeline3 {
        source: selector(#scroller3);
        time-range: 10s;
      }
      #element3 {
        animation-timeline: timeline3;
      }
  )CSS");
  GetDocument().body()->AppendChild(style_element);
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(HasObservers("scroller1"));
  EXPECT_FALSE(HasObservers("scroller2"));
  EXPECT_TRUE(HasObservers("scroller3"));
  EXPECT_TRUE(HasObservers("redefined"));
  EXPECT_FALSE(HasObservers("offset1"));
  EXPECT_TRUE(HasObservers("offset2"));

  // Remove the <style> element again.
  style_element->remove();
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(HasObservers("scroller1"));
  EXPECT_TRUE(HasObservers("scroller2"));
  EXPECT_FALSE(HasObservers("scroller3"));
  EXPECT_FALSE(HasObservers("redefined"));
  EXPECT_TRUE(HasObservers("offset1"));
  EXPECT_FALSE(HasObservers("offset2"));
}

TEST_F(CSSScrollTimelineTest, SharedTimelines) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim1 { to { top: 200px; } }
      @keyframes anim2 { to { left: 200px; } }
      @keyframes anim3 { to { right: 200px; } }
      @scroll-timeline timeline1 {
        source: selector(#scroller);
        time-range: 10s;
      }
      @scroll-timeline timeline2 {
        source: selector(#scroller);
        time-range: 10s;
      }
      #scroller {
        height: 100px;
        overflow: scroll;
      }
      #scroller > div {
        height: 200px;
      }
    </style>
    <div id=scroller><div></div></div>
    <main id=main></main>
  )HTML");
  // #scroller etc is created in a separate lifecycle phase to ensure that
  // we get a layout box for #scroller before the animations are started.

  Element* main = GetDocument().getElementById("main");
  ASSERT_TRUE(main);
  main->setInnerHTML(R"HTML(
    <style>
      #element1, #element2 {
        animation-name: anim1, anim2, anim3;
        animation-duration: 10s;
        animation-timeline: timeline1, timeline1, timeline2;
      }
    </style>
    <div id=element1></div>
    <div id=element2></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element1 = GetDocument().getElementById("element1");
  Element* element2 = GetDocument().getElementById("element2");
  ASSERT_TRUE(element1);
  ASSERT_TRUE(element2);
  HeapVector<Member<Animation>> animations1 = element1->getAnimations();
  HeapVector<Member<Animation>> animations2 = element2->getAnimations();
  EXPECT_EQ(3u, animations1.size());
  EXPECT_EQ(3u, animations2.size());

  // The animations associated with anim1 and anim2 should share the same
  // timeline instance, also across elements.
  EXPECT_EQ(animations1[0]->timeline(), animations1[1]->timeline());
  EXPECT_EQ(animations1[1]->timeline(), animations2[0]->timeline());
  EXPECT_EQ(animations2[0]->timeline(), animations2[1]->timeline());

  // The animation associated with anim3 uses a different timeline
  // from anim1/2.
  EXPECT_EQ(animations1[2]->timeline(), animations2[2]->timeline());

  EXPECT_NE(animations2[2]->timeline(), animations1[0]->timeline());
  EXPECT_NE(animations2[2]->timeline(), animations1[1]->timeline());
}

}  // namespace blink
