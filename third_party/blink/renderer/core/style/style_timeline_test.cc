// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_timeline.h"

#include <utility>

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "gin/public/v8_platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/timeline_inset.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_platform.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

template <typename Functor>
void RunOnDetachedTestingHeap(Functor&& functor) {
  HeapTestingPlatformAdapter platform_adapter(gin::V8Platform::Get());
  std::unique_ptr<NonMainThread> thread = NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread)
          .SetThreadNameForTest("style timeline gc test"));
  base::WaitableEvent done;
  PostCrossThreadTask(
      *thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](Functor* fn, v8::Platform* platform, base::WaitableEvent* done) {
            cppgc::StackStartMarker stack_start_marker;
            ThreadState::AttachCurrentThreadForTesting(platform,
                                                       stack_start_marker);
            (*fn)();
            ThreadState::DetachCurrentThread();
            done->Signal();
          },
          CrossThreadUnretained(&functor),
          CrossThreadUnretained(&platform_adapter),
          CrossThreadUnretained(&done)));
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
  done.Wait();
}

}  // namespace

TEST(StyleTimelineTest,
     SubscriptCopyAssignNameOverKeywordDuringIncrementalMarking) {
  test::TaskEnvironment task_environment;
  AtomicString timeline_name("--my-timeline");
  RunOnDetachedTestingHeap([&]() {
    Persistent<GCedHeapVector<StyleTimeline>> dst =
        MakeGarbageCollected<GCedHeapVector<StyleTimeline>>();
    dst->push_back(StyleTimeline(CSSValueID::kAuto));

    WeakPersistent<const ScopedCSSName> weak_name =
        MakeGarbageCollected<ScopedCSSName>(timeline_name, nullptr);
    GCedHeapVector<StyleTimeline>* src =
        MakeGarbageCollected<GCedHeapVector<StyleTimeline>>();
    src->push_back(StyleTimeline(weak_name.Get()));

    IncrementalMarkingTestDriver driver(ThreadState::Current());
    driver.StartGC();
    driver.TriggerMarkingSteps();

    (*dst)[0] = (*src)[0];

    driver.FinishGC();

    ASSERT_TRUE(weak_name);
    EXPECT_TRUE((*dst)[0].IsName());
    EXPECT_EQ(weak_name.Get(), (*dst)[0].GetScopedName());
    EXPECT_EQ(timeline_name, (*dst)[0].GetName());

    TestSupportingGC::PreciselyCollectGarbage();
    EXPECT_TRUE(weak_name);

    (*dst)[0] = StyleTimeline(CSSValueID::kNone);
    TestSupportingGC::PreciselyCollectGarbage();
    EXPECT_FALSE(weak_name);
  });
}

TEST(StyleTimelineTest,
     SubscriptMoveAssignNameOverScrollAndViewDuringIncrementalMarking) {
  test::TaskEnvironment task_environment;
  AtomicString name1("--timeline-1");
  AtomicString name2("--timeline-2");
  RunOnDetachedTestingHeap([&]() {
    Persistent<GCedHeapVector<StyleTimeline>> dst =
        MakeGarbageCollected<GCedHeapVector<StyleTimeline>>();
    dst->push_back(StyleTimeline(StyleTimeline::ScrollData(
        TimelineAxis::kBlock, TimelineScroller::kNearest)));
    dst->push_back(StyleTimeline(StyleTimeline::ViewData(
        TimelineAxis::kInline,
        TimelineInset(Length::Fixed(10), Length::Fixed(20)))));

    WeakPersistent<const ScopedCSSName> weak_name1 =
        MakeGarbageCollected<ScopedCSSName>(name1, nullptr);
    WeakPersistent<const ScopedCSSName> weak_name2 =
        MakeGarbageCollected<ScopedCSSName>(name2, nullptr);
    GCedHeapVector<StyleTimeline>* src =
        MakeGarbageCollected<GCedHeapVector<StyleTimeline>>();
    src->push_back(StyleTimeline(weak_name1.Get()));
    src->push_back(StyleTimeline(weak_name2.Get()));

    IncrementalMarkingTestDriver driver(ThreadState::Current());
    driver.StartGC();
    driver.TriggerMarkingSteps();

    (*dst)[0] = std::move((*src)[0]);
    (*dst)[1] = std::move((*src)[1]);

    driver.FinishGC();

    ASSERT_TRUE(weak_name1);
    ASSERT_TRUE(weak_name2);
    EXPECT_TRUE((*dst)[0].IsName());
    EXPECT_TRUE((*dst)[1].IsName());
    EXPECT_EQ(weak_name1.Get(), (*dst)[0].GetScopedName());
    EXPECT_EQ(weak_name2.Get(), (*dst)[1].GetScopedName());

    TestSupportingGC::PreciselyCollectGarbage();
    EXPECT_TRUE(weak_name1);
    EXPECT_TRUE(weak_name2);
  });
}

TEST(StyleTimelineTest,
     VectorCopyAssignNameOverKeywordDuringIncrementalMarking) {
  test::TaskEnvironment task_environment;
  AtomicString timeline_name("--inherited-timeline");
  RunOnDetachedTestingHeap([&]() {
    Persistent<GCedHeapVector<StyleTimeline>> dst =
        MakeGarbageCollected<GCedHeapVector<StyleTimeline>>();
    dst->push_back(StyleTimeline(CSSValueID::kAuto));

    WeakPersistent<const ScopedCSSName> weak_name =
        MakeGarbageCollected<ScopedCSSName>(timeline_name, nullptr);
    GCedHeapVector<StyleTimeline>* src =
        MakeGarbageCollected<GCedHeapVector<StyleTimeline>>();
    src->push_back(StyleTimeline(weak_name.Get()));

    IncrementalMarkingTestDriver driver(ThreadState::Current());
    driver.StartGC();
    driver.TriggerMarkingSteps();

    *dst = *src;

    driver.FinishGC();

    ASSERT_TRUE(weak_name);
    ASSERT_EQ(1u, dst->size());
    EXPECT_TRUE((*dst)[0].IsName());
    EXPECT_EQ(weak_name.Get(), (*dst)[0].GetScopedName());

    TestSupportingGC::PreciselyCollectGarbage();
    EXPECT_TRUE(weak_name);
  });
}

class StyleTimelineCssTest : public PageTestBase {};

TEST_F(StyleTimelineCssTest,
       AnimationTimelineInheritRetainedByTransitionAcrossGC) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { width: 10px; }
        to { width: 20px; }
      }
      #scroller {
        overflow: auto;
        scroll-timeline-name: --tl;
      }
      #parent {
        animation-timeline: --tl;
      }
      #child {
        animation-name: anim;
        animation-timeline: auto;
        transition: opacity 100s linear;
        opacity: 0;
      }
      #child.inherit {
        animation-timeline: inherit;
        opacity: 1;
      }
    </style>
    <div id="scroller">
      <div id="parent">
        <div id="child"></div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* parent = GetElementById("parent");
  Element* child = GetElementById("child");
  ASSERT_TRUE(parent);
  ASSERT_TRUE(child);

  child->setAttribute(html_names::kClassAttr, AtomicString("inherit"));
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(child->GetComputedStyle());
  ASSERT_TRUE(child->GetComputedStyle()->Animations());
  const HeapVector<StyleTimeline>& timelines =
      child->GetComputedStyle()->Animations()->TimelineList();
  ASSERT_EQ(1u, timelines.size());
  ASSERT_TRUE(timelines[0].IsName());
  EXPECT_EQ(AtomicString("--tl"), timelines[0].GetName());

  WeakPersistent<const ScopedCSSName> weak_name = timelines[0].GetScopedName();
  ASSERT_TRUE(weak_name);

  ASSERT_TRUE(child->GetElementAnimations());
  EXPECT_FALSE(child->GetElementAnimations()->Animations().empty());

  parent->setAttribute(html_names::kStyleAttr,
                       AtomicString("animation-timeline: none"));
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("animation-timeline: none"));
  UpdateAllLifecyclePhasesForTest();

  ThreadState::Current()->CollectAllGarbageForTesting();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(weak_name);

  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("animation-timeline: none; display: none"));
  UpdateAllLifecyclePhasesForTest();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(weak_name);
}

TEST_F(StyleTimelineCssTest,
       TimelineTriggerSourceInheritRetainedByTransitionAcrossGC) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { width: 10px; }
        to { width: 20px; }
      }
      #scroller {
        overflow: auto;
        scroll-timeline-name: --tl;
      }
      #parent {
        timeline-trigger-source: --tl;
      }
      #child {
        animation: anim 1s both;
        animation-trigger: --trigger;
        timeline-trigger-name: --trigger;
        timeline-trigger-source: auto;
        transition: opacity 100s linear;
        opacity: 0;
      }
      #child.inherit {
        timeline-trigger-source: inherit;
        opacity: 1;
      }
    </style>
    <div id="scroller">
      <div id="parent">
        <div id="child"></div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* parent = GetElementById("parent");
  Element* child = GetElementById("child");
  ASSERT_TRUE(parent);
  ASSERT_TRUE(child);

  child->setAttribute(html_names::kClassAttr, AtomicString("inherit"));
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(child->GetComputedStyle());
  ASSERT_TRUE(child->GetComputedStyle()->Animations());
  const HeapVector<StyleTimeline>& sources =
      child->GetComputedStyle()->Animations()->TimelineTriggerSourceList();
  ASSERT_EQ(1u, sources.size());
  ASSERT_TRUE(sources[0].IsName());
  EXPECT_EQ(AtomicString("--tl"), sources[0].GetName());

  WeakPersistent<const ScopedCSSName> weak_name = sources[0].GetScopedName();
  ASSERT_TRUE(weak_name);

  ASSERT_TRUE(child->NamedTriggers());
  EXPECT_FALSE(child->NamedTriggers()->empty());
  ASSERT_TRUE(child->GetElementAnimations());
  EXPECT_FALSE(child->GetElementAnimations()->Animations().empty());

  parent->setAttribute(html_names::kStyleAttr,
                       AtomicString("timeline-trigger-source: none"));
  child->setAttribute(
      html_names::kStyleAttr,
      AtomicString("animation-name: none; timeline-trigger-source: none"));
  UpdateAllLifecyclePhasesForTest();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(weak_name);

  child->setAttribute(
      html_names::kStyleAttr,
      AtomicString("animation-name: none; timeline-trigger-source: none; "
                   "display: none"));
  UpdateAllLifecyclePhasesForTest();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(weak_name);
}

}  // namespace blink
