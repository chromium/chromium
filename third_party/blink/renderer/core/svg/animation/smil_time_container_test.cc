// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"
#include "third_party/blink/renderer/core/svg/svg_set_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

class SMILTimeContainerTest : public PageTestBase {
 public:
  SMILTimeContainerTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetUp();
  }

  void Load(std::string_view data) {
    auto params = WebNavigationParams::CreateWithHTMLStringForTesting(
        data, KURL("http://example.com"));
    GetFrame().Loader().CommitNavigation(std::move(params),
                                         nullptr /* extra_data */);
    GetAnimationClock().OverrideDynamicClockForTesting(
        platform()->GetTickClock());
    GetAnimationClock().SetAllowedToDynamicallyUpdateTime(false);
    GetDocument().Timeline().ResetForTesting();
  }

  void StepTime(base::TimeDelta delta) {
    AnimationClock::NotifyTaskStart();
    AdvanceClock(delta);
    GetAnimationClock().SetAllowedToDynamicallyUpdateTime(false);
    GetAnimationClock().UpdateTime(platform()->NowTicks());
  }
};

TEST_F(SMILTimeContainerTest, ServiceAnimationsFlushesPendingSynchronizations) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green"/>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(0, rect->height()->CurrentValue()->Value(length_context));

  // Insert an animation: <set attributeName="height" to="100"/> of the <rect>.
  auto* animation = MakeGarbageCollected<SVGSetElement>(GetDocument());
  animation->setAttribute(svg_names::kAttributeTypeAttr, AtomicString("XML"));
  animation->setAttribute(svg_names::kAttributeNameAttr,
                          AtomicString("height"));
  animation->setAttribute(svg_names::kToAttr, AtomicString("100"));
  rect->appendChild(animation);

  // Frame callback before the synchronization timer fires.
  SVGDocumentExtensions::ServiceSmilOnAnimationFrame(GetDocument());
  SVGDocumentExtensions::ServiceWebAnimationsOnAnimationFrame(GetDocument());

  // The frame callback should have flushed any pending updates.
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(500));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(0.5), time_container->Elapsed());
}

TEST_F(SMILTimeContainerTest, ServiceAnimationsResyncOnLag) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="100" fill="blue">
        <animate begin="0s" dur="5min" repeatCount="indefinite"
                 attributeName="width" from="0" to="100"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());

  // Step an hour ahead. Since the animation starts generating frame callbacks
  // at t=0s it will auto-suspend after 1 minute.
  StepTime(base::Minutes(60));
  SVGDocumentExtensions::ServiceSmilOnAnimationFrame(GetDocument());

  EXPECT_EQ(SMILTime::FromSecondsD(60), time_container->Elapsed());
}

TEST_F(SMILTimeContainerTest, ServiceAnimationsNoResyncAfterFutureFrame) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="100" fill="blue">
        <animate begin="50min" dur="5min" repeatCount="indefinite"
                 attributeName="width" from="0" to="100"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());

  // Like PageAnimator::PostAnimate(). Allows the clock to adjust for/during
  // the timer delay.
  GetAnimationClock().SetAllowedToDynamicallyUpdateTime(true);

  // Step 30 seconds into the first repeat of the animations interval. Since
  // the animation doesn't start generating frame callbacks until t=50min it
  // will not auto-suspend.
  const base::TimeDelta lag = base::Minutes(50) + base::Seconds(30);
  StepTime(lag);
  SVGDocumentExtensions::ServiceSmilOnAnimationFrame(GetDocument());

  EXPECT_EQ(SMILTime::FromTimeDelta(lag), time_container->Elapsed());
}

TEST_F(SMILTimeContainerTest, ServiceAnimationsNoSuspendOnAnimationSync) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="100" fill="blue"/>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());

  // Like PageAnimator::PostAnimate(). Allows the clock to adjust for/during
  // the timer delay.
  GetAnimationClock().SetAllowedToDynamicallyUpdateTime(true);

  // Step an hour ahead. There are no animations running, but the timeline is
  // active.
  const base::TimeDelta elapsed = base::Minutes(60);
  StepTime(elapsed);

  // Insert an animation element:
  //  <animate begin="0s" dur="5min" repeatCount="indefinite"
  //           attributeName="width" from="0" to="100"/>
  auto* animation = GetDocument().CreateRawElement(svg_names::kAnimateTag);
  animation->setAttribute(svg_names::kBeginAttr, AtomicString("0s"));
  animation->setAttribute(svg_names::kDurAttr, AtomicString("5min"));
  animation->setAttribute(svg_names::kRepeatCountAttr,
                          AtomicString("indefinite"));
  animation->setAttribute(svg_names::kAttributeNameAttr, AtomicString("width"));
  animation->setAttribute(svg_names::kFromAttr, AtomicString("0"));
  animation->setAttribute(svg_names::kToAttr, AtomicString("100"));
  svg_root->AppendChild(animation);

  SVGDocumentExtensions::ServiceSmilOnAnimationFrame(GetDocument());

  // The timeline should not have been suspended (i.e the time elapsed on the
  // timeline should equal the currently elapsed time).
  EXPECT_EQ(SMILTime::FromTimeDelta(elapsed), time_container->Elapsed());
}

class ContentLoadedEventListener final : public NativeEventListener {
 public:
  using CallbackType = base::OnceCallback<void(Document&)>;
  explicit ContentLoadedEventListener(CallbackType callback)
      : callback_(std::move(callback)) {}

  void Invoke(ExecutionContext* execution_context, Event*) override {
    std::move(callback_).Run(
        *To<LocalDOMWindow>(execution_context)->document());
  }

 private:
  CallbackType callback_;
};

class SMILTimeContainerAnimationPolicyOnceTest : public PageTestBase {
 public:
  SMILTimeContainerAnimationPolicyOnceTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetupPageWithClients(nullptr, nullptr, &OverrideSettings);
  }

  void Load(std::string_view data) {
    auto params = WebNavigationParams::CreateWithHTMLStringForTesting(
        data, KURL("http://example.com"));
    GetFrame().Loader().CommitNavigation(std::move(params),
                                         nullptr /* extra_data */);
    GetAnimationClock().ResetTimeForTesting();
    GetAnimationClock().SetAllowedToDynamicallyUpdateTime(false);
    GetDocument().Timeline().ResetForTesting();
  }

  void StepTime(base::TimeDelta delta) {
    FastForwardBy(delta);
    current_time_ += delta;
    GetAnimationClock().UpdateTime(current_time_);
    SVGDocumentExtensions::ServiceSmilOnAnimationFrame(GetDocument());
    SVGDocumentExtensions::ServiceWebAnimationsOnAnimationFrame(GetDocument());
  }

  void OnContentLoaded(base::OnceCallback<void(Document&)> callback) {
    GetFrame().DomWindow()->addEventListener(
        event_type_names::kDOMContentLoaded,
        MakeGarbageCollected<ContentLoadedEventListener>(std::move(callback)));
  }

 private:
  static void OverrideSettings(Settings& settings) {
    settings.SetImageAnimationPolicy(
        mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAnimateOnce);
  }

  base::TimeTicks current_time_;
};

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, NoAction) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(2500));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(500));
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(3), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, SetElapsedAfterStart) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  time_container->SetElapsed(SMILTime::FromSecondsD(5.5));
  EXPECT_EQ(SMILTime::FromSecondsD(5.5), time_container->Elapsed());
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(2000));
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(8.5), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, SetElapsedBeforeStart) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  OnContentLoaded(WTF::BindOnce([](Document& document) {
    auto* svg_root =
        To<SVGSVGElement>(document.getElementById(AtomicString("container")));
    ASSERT_TRUE(svg_root);
    auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
    ASSERT_TRUE(rect);
    SVGLengthContext length_context(rect);

    SMILTimeContainer* time_container = svg_root->TimeContainer();
    EXPECT_FALSE(time_container->IsStarted());
    EXPECT_FALSE(time_container->IsPaused());
    time_container->SetElapsed(SMILTime::FromSecondsD(5.5));
    EXPECT_EQ(0, rect->height()->CurrentValue()->Value(length_context));
  }));
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(5.5), time_container->Elapsed());
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(2000));
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(8.5), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, PauseAfterStart) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  time_container->Pause();
  EXPECT_TRUE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(1.5), time_container->Elapsed());

  time_container->Unpause();
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(1.5), time_container->Elapsed());

  StepTime(base::Milliseconds(4000));
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(4.5), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, PauseBeforeStart) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  OnContentLoaded(WTF::BindOnce([](Document& document) {
    auto* svg_root =
        To<SVGSVGElement>(document.getElementById(AtomicString("container")));
    ASSERT_TRUE(svg_root);
    auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
    ASSERT_TRUE(rect);
    SVGLengthContext length_context(rect);

    SMILTimeContainer* time_container = svg_root->TimeContainer();
    EXPECT_FALSE(time_container->IsStarted());
    EXPECT_FALSE(time_container->IsPaused());
    time_container->Pause();
    EXPECT_TRUE(time_container->IsPaused());
    EXPECT_EQ(0, rect->height()->CurrentValue()->Value(length_context));
  }));
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_TRUE(time_container->IsPaused());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(SMILTime::FromSecondsD(0), time_container->Elapsed());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  time_container->Unpause();
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(0), time_container->Elapsed());

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(2500));
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(3), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, PauseAndSetElapsedAfterStart) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  time_container->Pause();
  EXPECT_TRUE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(1.5), time_container->Elapsed());

  time_container->SetElapsed(SMILTime::FromSecondsD(0.5));
  EXPECT_EQ(SMILTime::FromSecondsD(0.5), time_container->Elapsed());

  time_container->Unpause();
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(0.5), time_container->Elapsed());

  StepTime(base::Milliseconds(4000));
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(3.5), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest,
       PauseAndSetElapsedBeforeStart) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  OnContentLoaded(WTF::BindOnce([](Document& document) {
    auto* svg_root =
        To<SVGSVGElement>(document.getElementById(AtomicString("container")));
    ASSERT_TRUE(svg_root);
    auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
    ASSERT_TRUE(rect);
    SVGLengthContext length_context(rect);

    SMILTimeContainer* time_container = svg_root->TimeContainer();
    EXPECT_FALSE(time_container->IsStarted());
    EXPECT_FALSE(time_container->IsPaused());
    time_container->Pause();
    EXPECT_TRUE(time_container->IsPaused());
    time_container->SetElapsed(SMILTime::FromSecondsD(1.5));
    EXPECT_TRUE(time_container->IsPaused());
    EXPECT_EQ(0, rect->height()->CurrentValue()->Value(length_context));
  }));
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_TRUE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(1.5), time_container->Elapsed());
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(SMILTime::FromSecondsD(1.5), time_container->Elapsed());
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  time_container->Unpause();
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(1.5), time_container->Elapsed());

  StepTime(base::Milliseconds(2000));
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(2000));
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(4.5), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, PauseAndResumeBeforeStart) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  OnContentLoaded(WTF::BindOnce([](Document& document) {
    auto* svg_root =
        To<SVGSVGElement>(document.getElementById(AtomicString("container")));
    ASSERT_TRUE(svg_root);
    auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
    ASSERT_TRUE(rect);
    SVGLengthContext length_context(rect);

    SMILTimeContainer* time_container = svg_root->TimeContainer();
    EXPECT_FALSE(time_container->IsStarted());
    EXPECT_FALSE(time_container->IsPaused());
    time_container->Pause();
    EXPECT_TRUE(time_container->IsPaused());
    time_container->Unpause();
    EXPECT_FALSE(time_container->IsPaused());
    EXPECT_EQ(0, rect->height()->CurrentValue()->Value(length_context));
  }));
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(2500));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(500));
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(3), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, PauseAndResumeAfterSuspended) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(0), time_container->Elapsed());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(1.0), time_container->Elapsed());
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(2.0), time_container->Elapsed());
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(SMILTime::FromSecondsD(3.0), time_container->Elapsed());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  time_container->Pause();
  EXPECT_TRUE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(3.0), time_container->Elapsed());

  time_container->Unpause();
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(3.0), time_container->Elapsed());

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(4.0), time_container->Elapsed());
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(5.0), time_container->Elapsed());
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(6.0), time_container->Elapsed());
}

TEST_F(SMILTimeContainerAnimationPolicyOnceTest, SetElapsedAfterSuspended) {
  Load(R"HTML(
    <svg id="container">
      <rect width="100" height="0" fill="green">
        <animate begin="0s" dur="3s" repeatCount="indefinite"
                 attributeName="height" values="30;50;100" calcMode="discrete"/>
      </rect>
    </svg>
  )HTML");
  platform()->RunUntilIdle();

  auto* svg_root = To<SVGSVGElement>(GetElementById("container"));
  ASSERT_TRUE(svg_root);
  auto* rect = Traversal<SVGRectElement>::FirstChild(*svg_root);
  ASSERT_TRUE(rect);
  SVGLengthContext length_context(rect);

  SMILTimeContainer* time_container = svg_root->TimeContainer();
  EXPECT_TRUE(time_container->IsStarted());
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(0), time_container->Elapsed());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(1.0), time_container->Elapsed());
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(2.0), time_container->Elapsed());
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(SMILTime::FromSecondsD(3.0), time_container->Elapsed());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  time_container->SetElapsed(SMILTime::FromSecondsD(5.5));
  EXPECT_FALSE(time_container->IsPaused());
  EXPECT_EQ(SMILTime::FromSecondsD(5.5), time_container->Elapsed());
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(6.5), time_container->Elapsed());
  EXPECT_EQ(30, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1000));
  EXPECT_EQ(SMILTime::FromSecondsD(7.5), time_container->Elapsed());
  EXPECT_EQ(50, rect->height()->CurrentValue()->Value(length_context));

  StepTime(base::Milliseconds(1500));
  EXPECT_EQ(100, rect->height()->CurrentValue()->Value(length_context));
  EXPECT_EQ(SMILTime::FromSecondsD(8.5), time_container->Elapsed());
}

}  // namespace
}  // namespace blink
