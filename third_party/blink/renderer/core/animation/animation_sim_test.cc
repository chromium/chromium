// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/exception_state_matchers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class AnimationSimTest : public SimTest {};

TEST_F(AnimationSimTest, CustomPropertyBaseComputedStyle) {
  // This is a regression test for bug where custom property animations failed
  // to disable the baseComputedStyle optimisation. When custom property
  // animations are in effect we lose the guarantee that the baseComputedStyle
  // optimisation relies on where the non-animated style rules always produce
  // the same ComputedStyle. This is not the case if they use var() references
  // to custom properties that are being animated.
  // The bug was that we never cleared the existing baseComputedStyle during a
  // custom property animation so the stale ComputedStyle object would hang
  // around and not be valid in the exit frame of the next custom property
  // animation.

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id=\"target\"></div>");

  Element* target = GetElementById("target");

  // CSS.registerProperty({
  //   name: '--x',
  //   syntax: '<percentage>',
  //   initialValue: '0%',
  //   inherits: false
  // })
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<percentage>", "0%",
                                     false);

  DummyExceptionStateForTesting exception_state;
  // target.style.setProperty('--x', '100%');
  target->style()->setProperty(GetDocument().GetExecutionContext(), "--x",
                               "100%", g_empty_string, exception_state);
  EXPECT_THAT(exception_state, HadNoException());

  // target.animate({'--x': '100%'}, 1000);
  auto* keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetCSSPropertyValue(AtomicString("--x"), "100%",
                                Window().GetSecureContextMode(),
                                GetDocument().ElementSheet().Contents());
  StringKeyframeVector keyframes;
  keyframes.push_back(keyframe);
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);

  auto* keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      target, MakeGarbageCollected<StringKeyframeEffectModel>(keyframes),
      timing);
  target->GetDocument().Timeline().Play(keyframe_effect);

  // This sets the baseComputedStyle on the animation exit frame.
  Compositor().BeginFrame(1);
  Compositor().BeginFrame(1);

  // target.style.setProperty('--x', '0%');
  target->style()->setProperty(GetDocument().GetExecutionContext(), "--x", "0%",
                               g_empty_string, exception_state);
  EXPECT_THAT(exception_state, HadNoException());

  // target.animate({'--x': '100%'}, 1000);
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetCSSPropertyValue(AtomicString("--x"), "100%",
                                Window().GetSecureContextMode(),
                                GetDocument().ElementSheet().Contents());
  keyframes.clear();
  keyframes.push_back(std::move(keyframe));
  timing = Timing();
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);

  keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      target, MakeGarbageCollected<StringKeyframeEffectModel>(keyframes),
      timing);
  target->GetDocument().Timeline().Play(keyframe_effect);

  // This (previously) would not clear the existing baseComputedStyle and would
  // crash on the equality assertion in the exit frame when it tried to update
  // it.
  Compositor().BeginFrame(1);
  Compositor().BeginFrame(1);
}

// An animation that doesn't tick on the main thread, e.g. because none of its
// values can change while it's in play, still needs a frame after it starts to
// dispatch animationstart, which isn't dispatched while it's pending.
TEST_F(AnimationSimTest, NonTickingAnimationDispatchesStartEventOnNextFrame) {
  class CountingEventListener final : public NativeEventListener {
   public:
    void Invoke(ExecutionContext*, Event*) override { ++count_; }
    int count() const { return count_; }

   private:
    int count_ = 0;
  };

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      @keyframes anim {
        from { opacity: 0.5; }
        to { opacity: 0.5; }
      }
    </style>
    <div id="target"></div>
  )HTML");
  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* target = GetElementById("target");
  auto* start_listener = MakeGarbageCollected<CountingEventListener>();
  target->addEventListener(event_type_names::kAnimationstart, start_listener);

  // Create the animation outside of a frame, e.g. as a forced style update
  // would.
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("animation: anim 10s"));
  GetDocument().UpdateStyleAndLayoutTree();
  ElementAnimations* element_animations = target->GetElementAnimations();
  ASSERT_TRUE(element_animations);
  ASSERT_EQ(1u, element_animations->Animations().size());
  Animation* animation = element_animations->Animations().begin()->key;
  EXPECT_TRUE(animation->pending());

  // The animation starts during this frame's paint, after events are
  // dispatched.
  ASSERT_TRUE(Compositor().NeedsBeginFrame());
  Compositor().BeginFrame();
  EXPECT_FALSE(animation->pending());
  EXPECT_TRUE(animation->AnimationHasNoEffect());
  EXPECT_EQ(0, start_listener->count());

  // The animation doesn't tick, but it needs one more frame to dispatch
  // animationstart.
  ASSERT_TRUE(Compositor().NeedsBeginFrame());
  Compositor().BeginFrame();
  EXPECT_EQ(1, start_listener->count());
  // It still doesn't need to tick until it ends.
  EXPECT_GT(animation->TimeToEffectChange().value().InSecondsF(), 9);
}

// Regression test for crbug.com/386304676: when a `display` allow-discrete
// transition (e.g. closing a `<dialog>` with `::backdrop`) reaches its end time
// during an on-demand style update before `BeginMainFrame`, the completed
// transitions must not be canceled when `display` becomes `none`.
TEST_F(AnimationSimTest, DisplayNoneTransitionFinishedOnDemand) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      dialog {
        opacity: 0;
        scale: 0.9;
        transition:
          display 200ms allow-discrete,
          opacity 200ms,
          overlay 200ms allow-discrete,
          scale 200ms;
      }
      dialog[open] {
        opacity: 1;
        scale: 1;
      }
      dialog::backdrop {
        background: rgba(0, 0, 0, 0.5);
        opacity: 0;
        transition:
          display 200ms allow-discrete,
          opacity 200ms,
          overlay 200ms allow-discrete;
      }
      dialog[open]::backdrop {
        opacity: 1;
      }
    </style>
    <dialog id="dialog">Hello</dialog>
    <div id="result"></div>
    <script>
      const dialog = document.getElementById('dialog');
      window.state = {
        resolved: [],
        rejected: [],
        transitionend: [],
        transitioncancel: [],
        finish: [],
        cancel: [],
        anims: [],
      };
      const keyFor = (a) =>
        (a.effect.pseudoElement || '') + ':' + a.transitionProperty;

      dialog.addEventListener('transitionend', (e) => {
        window.state.transitionend.push(e.pseudoElement + ':' + e.propertyName);
      });
      dialog.addEventListener('transitioncancel', (e) => {
        window.state.transitioncancel.push(
            e.pseudoElement + ':' + e.propertyName);
      });

      window.startClose = () => {
        dialog.close();
        window.state.anims = dialog.getAnimations({subtree: true});
        for (const a of window.state.anims) {
          const k = keyFor(a);
          a.finished.then(
              () => window.state.resolved.push(k),
              (err) => window.state.rejected.push(k + ':' + err.name));
          a.addEventListener('finish', () => window.state.finish.push(k));
          a.addEventListener('cancel', () => window.state.cancel.push(k));
        }
      };

      window.report = () => {
        const s = window.state;
        const summary = [
          'count=' + s.anims.length,
          'resolved=' + s.resolved.slice().sort().join(','),
          'rejected=' + s.rejected.slice().sort().join(','),
          'transitionend=' + s.transitionend.slice().sort().join(','),
          'transitioncancel=' + s.transitioncancel.slice().sort().join(','),
          'finish=' + s.finish.slice().sort().join(','),
          'cancel=' + s.cancel.slice().sort().join(','),
          'states=' +
              s.anims.map((a) => keyFor(a) + '=' + a.playState)
                  .sort()
                  .join(','),
        ].join('\n');
        document.getElementById('result').textContent = summary;
      };
    </script>
  )HTML");

  Compositor().BeginFrame(0.016);
  MainFrame().ExecuteScript(
      WebScriptSource("document.getElementById('dialog').showModal();"));
  Compositor().BeginFrame(0.016);

  MainFrame().ExecuteScript(WebScriptSource("window.startClose();"));
  Compositor().BeginFrame(0.016);

  // Advance the animation clock past the 200ms transition duration without a
  // BeginMainFrame, then trigger an on-demand style update.
  base::TimeTicks now = GetDocument().GetAnimationClock().CurrentTime();
  GetDocument().GetAnimationClock().UpdateTime(now + base::Milliseconds(250));

  MainFrame().ExecuteScript(WebScriptSource(
      "window.displayAfterFlush = "
      "getComputedStyle(document.getElementById('dialog')).display;"));
  // A frame must be scheduled to resolve promises and dispatch transitionend.
  EXPECT_TRUE(Compositor().NeedsBeginFrame());

  // Pump frames so the animation-frame update, microtasks, and queued
  // playback events all complete.
  Compositor().BeginFrame(0.016);
  test::RunPendingTasks();
  if (Compositor().NeedsBeginFrame()) {
    Compositor().BeginFrame(0.016);
    test::RunPendingTasks();
  }

  MainFrame().ExecuteScript(
      WebScriptSource("window.report(); "
                      "document.getElementById('result').textContent += "
                      "'\\ndisplay=' + window.displayAfterFlush;"));

  const String expected =
      "count=5\n"
      "resolved=::backdrop:opacity,:display,:opacity,:overlay,:scale\n"
      "rejected=\n"
      "transitionend=::backdrop:opacity,:display,:opacity,:overlay,:scale\n"
      "transitioncancel=\n"
      "finish=::backdrop:opacity,:display,:opacity,:overlay,:scale\n"
      "cancel=\n"
      "states=::backdrop:opacity=finished,:display=finished,"
      ":opacity=finished,:overlay=finished,:scale=finished\n"
      "display=none";
  EXPECT_EQ(expected, GetElementById("result")->textContent());
}

// Regression test for crbug.com/386304676: even when a `display` allow-discrete
// transition finishes cleanly on a `BeginMainFrame`, `CSSAnimations::Cancel()`
// during the same frame's style recalc must not cancel the finished transitions
// or fire spurious `cancel` events.
TEST_F(AnimationSimTest, DisplayNoneTransitionFinishedOnAnimationFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      dialog {
        opacity: 0;
        transition:
          display 200ms allow-discrete,
          opacity 200ms,
          overlay 200ms allow-discrete;
      }
      dialog[open] {
        opacity: 1;
      }
      dialog::backdrop {
        opacity: 0;
        transition: opacity 200ms;
      }
      dialog[open]::backdrop {
        opacity: 1;
      }
    </style>
    <dialog id="dialog">Hello</dialog>
    <div id="result"></div>
    <script>
      const dialog = document.getElementById('dialog');
      window.state = {
        resolved: [],
        rejected: [],
        transitionend: [],
        transitioncancel: [],
        finish: [],
        cancel: [],
        anims: [],
      };
      const keyFor = (a) =>
        (a.effect.pseudoElement || '') + ':' + a.transitionProperty;

      dialog.addEventListener('transitionend', (e) => {
        window.state.transitionend.push(e.pseudoElement + ':' + e.propertyName);
      });
      dialog.addEventListener('transitioncancel', (e) => {
        window.state.transitioncancel.push(
            e.pseudoElement + ':' + e.propertyName);
      });

      window.startClose = () => {
        dialog.close();
        window.state.anims = dialog.getAnimations({subtree: true});
        for (const a of window.state.anims) {
          const k = keyFor(a);
          a.finished.then(
              () => window.state.resolved.push(k),
              (err) => window.state.rejected.push(k + ':' + err.name));
          a.addEventListener('finish', () => window.state.finish.push(k));
          a.addEventListener('cancel', () => window.state.cancel.push(k));
        }
      };

      window.report = () => {
        const s = window.state;
        document.getElementById('result').textContent = [
          'resolved=' + s.resolved.slice().sort().join(','),
          'rejected=' + s.rejected.slice().sort().join(','),
          'transitionend=' + s.transitionend.slice().sort().join(','),
          'transitioncancel=' + s.transitioncancel.slice().sort().join(','),
          'finish=' + s.finish.slice().sort().join(','),
          'cancel=' + s.cancel.slice().sort().join(','),
          'states=' +
              s.anims.map((a) => keyFor(a) + '=' + a.playState)
                  .sort()
                  .join(','),
        ].join('\n');
      };
    </script>
  )HTML");

  Compositor().BeginFrame(0.016);
  MainFrame().ExecuteScript(
      WebScriptSource("document.getElementById('dialog').showModal();"));
  Compositor().BeginFrame(0.016);

  MainFrame().ExecuteScript(WebScriptSource("window.startClose();"));
  Compositor().BeginFrame(0.016);

  // Advance straight into a BeginMainFrame past the 200ms duration without an
  // intermediate on-demand style flush.
  base::TimeTicks now = GetDocument().GetAnimationClock().CurrentTime();
  GetDocument().GetAnimationClock().UpdateTime(now + base::Milliseconds(250));
  Compositor().BeginFrame(0.016);
  test::RunPendingTasks();
  if (Compositor().NeedsBeginFrame()) {
    Compositor().BeginFrame(0.016);
    test::RunPendingTasks();
  }

  MainFrame().ExecuteScript(WebScriptSource("window.report();"));
  const String expected =
      "resolved=::backdrop:opacity,:display,:opacity,:overlay\n"
      "rejected=\n"
      "transitionend=::backdrop:opacity,:display,:opacity,:overlay\n"
      "transitioncancel=\n"
      "finish=::backdrop:opacity,:display,:opacity,:overlay\n"
      "cancel=\n"
      "states=::backdrop:opacity=finished,:display=finished,"
      ":opacity=finished,:overlay=finished";
  EXPECT_EQ(expected, GetElementById("result")->textContent());
}

// Verify that when a shorter `display` transition completes during an on-demand
// style flush while a longer transition is still running, the completed
// transition finishes cleanly while the running transition is still canceled.
TEST_F(AnimationSimTest, DisplayNoneTransitionPartialFinishOnDemand) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      #target {
        display: block;
        opacity: 1;
        transition:
          display 200ms allow-discrete,
          opacity 500ms;
      }
      #target.hidden {
        display: none;
        opacity: 0;
      }
    </style>
    <div id="target">Hello</div>
    <div id="result"></div>
    <script>
      const target = document.getElementById('target');
      window.state = {
        resolved: [],
        rejected: [],
        transitionend: [],
        transitioncancel: [],
        finish: [],
        cancel: [],
        anims: [],
      };
      target.addEventListener('transitionend', (e) => {
        window.state.transitionend.push(e.propertyName);
      });
      target.addEventListener('transitioncancel', (e) => {
        window.state.transitioncancel.push(e.propertyName);
      });

      window.startHide = () => {
        target.classList.add('hidden');
        window.state.anims = target.getAnimations();
        for (const a of window.state.anims) {
          const p = a.transitionProperty;
          a.finished.then(
              () => window.state.resolved.push(p),
              (err) => window.state.rejected.push(p + ':' + err.name));
          a.addEventListener('finish', () => window.state.finish.push(p));
          a.addEventListener('cancel', () => window.state.cancel.push(p));
        }
      };

      window.report = () => {
        const s = window.state;
        document.getElementById('result').textContent = [
          'resolved=' + s.resolved.slice().sort().join(','),
          'rejected=' + s.rejected.slice().sort().join(','),
          'transitionend=' + s.transitionend.slice().sort().join(','),
          'transitioncancel=' + s.transitioncancel.slice().sort().join(','),
          'finish=' + s.finish.slice().sort().join(','),
          'cancel=' + s.cancel.slice().sort().join(','),
          'states=' +
              s.anims.map((a) => a.transitionProperty + '=' + a.playState)
                  .sort()
                  .join(','),
        ].join('\n');
      };
    </script>
  )HTML");

  Compositor().BeginFrame(0.016);
  MainFrame().ExecuteScript(WebScriptSource("window.startHide();"));
  Compositor().BeginFrame(0.016);

  // Advance past display (200ms) but before opacity (500ms), then flush style.
  base::TimeTicks now = GetDocument().GetAnimationClock().CurrentTime();
  GetDocument().GetAnimationClock().UpdateTime(now + base::Milliseconds(250));
  MainFrame().ExecuteScript(WebScriptSource(
      "getComputedStyle(document.getElementById('target')).display;"));
  EXPECT_TRUE(Compositor().NeedsBeginFrame());

  Compositor().BeginFrame(0.016);
  test::RunPendingTasks();
  if (Compositor().NeedsBeginFrame()) {
    Compositor().BeginFrame(0.016);
    test::RunPendingTasks();
  }

  MainFrame().ExecuteScript(WebScriptSource("window.report();"));
  const String expected =
      "resolved=display\n"
      "rejected=opacity:AbortError\n"
      "transitionend=display\n"
      "transitioncancel=opacity\n"
      "finish=display\n"
      "cancel=opacity\n"
      "states=display=finished,opacity=idle";
  EXPECT_EQ(expected, GetElementById("result")->textContent());
}

// Verify that a `CSSAnimation` with `fill: none` keeping an element with base
// style `display: none` visible does not get canceled when it finishes, either
// via an on-demand style flush or on a `BeginMainFrame`.
TEST_F(AnimationSimTest, DisplayNoneCSSAnimationFinished) {
  for (bool on_demand_flush : {true, false}) {
    SCOPED_TRACE(on_demand_flush ? "on_demand_flush" : "animation_frame");
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(R"HTML(
      <style>
        @keyframes fade-out {
          from { display: block; opacity: 1; }
          to { display: block; opacity: 0; }
        }
        #target {
          display: block;
        }
        #target.closing {
          display: none;
          animation: fade-out 200ms;
        }
      </style>
      <div id="target">Hello</div>
      <div id="result"></div>
      <script>
        const target = document.getElementById('target');
        window.state = {
          resolved: 0,
          rejected: [],
          animationend: 0,
          animationcancel: 0,
          finish: 0,
          cancel: 0,
          anim: null,
        };
        target.addEventListener('animationend', () => {
          window.state.animationend++;
        });
        target.addEventListener('animationcancel', () => {
          window.state.animationcancel++;
        });
        window.startAnim = () => {
          target.classList.add('closing');
          const anim = target.getAnimations()[0];
          window.state.anim = anim;
          anim.finished.then(
              () => window.state.resolved++,
              (err) => window.state.rejected.push(err.name));
          anim.addEventListener('finish', () => window.state.finish++);
          anim.addEventListener('cancel', () => window.state.cancel++);
        };
        window.report = () => {
          const s = window.state;
          document.getElementById('result').textContent = [
            'resolved=' + s.resolved,
            'rejected=' + s.rejected.join(','),
            'animationend=' + s.animationend,
            'animationcancel=' + s.animationcancel,
            'finish=' + s.finish,
            'cancel=' + s.cancel,
            'playState=' + s.anim.playState,
          ].join('\n');
        };
      </script>
    )HTML");

    Compositor().BeginFrame(0.016);
    MainFrame().ExecuteScript(WebScriptSource("window.startAnim();"));
    Compositor().BeginFrame(0.016);

    base::TimeTicks now = GetDocument().GetAnimationClock().CurrentTime();
    GetDocument().GetAnimationClock().UpdateTime(now + base::Milliseconds(250));
    if (on_demand_flush) {
      MainFrame().ExecuteScript(WebScriptSource(
          "getComputedStyle(document.getElementById('target')).display;"));
      EXPECT_TRUE(Compositor().NeedsBeginFrame());
    }

    Compositor().BeginFrame(0.016);
    test::RunPendingTasks();
    if (Compositor().NeedsBeginFrame()) {
      Compositor().BeginFrame(0.016);
      test::RunPendingTasks();
    }

    MainFrame().ExecuteScript(WebScriptSource("window.report();"));
    const String expected =
        "resolved=1\n"
        "rejected=\n"
        "animationend=1\n"
        "animationcancel=0\n"
        "finish=1\n"
        "cancel=0\n"
        "playState=finished";
    EXPECT_EQ(expected, GetElementById("result")->textContent());
  }
}

// Verify that retargeting a property or removing it from `transition-property`
// during an on-demand style flush after the previous transition has already
// reached its end finishes the previous transition instead of canceling it.
TEST_F(AnimationSimTest, TransitionRetargetOrRemoveAfterFinishOnDemand) {
  for (bool remove_transition_property : {false, true}) {
    SCOPED_TRACE(remove_transition_property ? "remove_transition_property"
                                            : "retarget_property");
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(R"HTML(
      <style>
        #target {
          opacity: 1;
          transition: opacity 200ms;
        }
      </style>
      <div id="target">Hello</div>
      <div id="result"></div>
      <script>
        const target = document.getElementById('target');
        window.state = {
          resolved: 0,
          rejected: [],
          transitionend: 0,
          transitioncancel: 0,
          finish: 0,
          cancel: 0,
          anim: null,
        };
        target.addEventListener('transitionend', () => {
          window.state.transitionend++;
        });
        target.addEventListener('transitioncancel', () => {
          window.state.transitioncancel++;
        });
        window.startTransition = () => {
          target.style.opacity = '0';
          const anim = target.getAnimations()[0];
          window.state.anim = anim;
          anim.finished.then(
              () => window.state.resolved++,
              (err) => window.state.rejected.push(err.name));
          anim.addEventListener('finish', () => window.state.finish++);
          anim.addEventListener('cancel', () => window.state.cancel++);
        };
        window.report = () => {
          const s = window.state;
          document.getElementById('result').textContent = [
            'resolved=' + s.resolved,
            'rejected=' + s.rejected.join(','),
            'transitionend=' + s.transitionend,
            'transitioncancel=' + s.transitioncancel,
            'finish=' + s.finish,
            'cancel=' + s.cancel,
            'playState=' + s.anim.playState,
          ].join('\n');
        };
      </script>
    )HTML");

    Compositor().BeginFrame(0.016);
    MainFrame().ExecuteScript(WebScriptSource("window.startTransition();"));
    Compositor().BeginFrame(0.016);

    base::TimeTicks now = GetDocument().GetAnimationClock().CurrentTime();
    GetDocument().GetAnimationClock().UpdateTime(now + base::Milliseconds(250));
    if (remove_transition_property) {
      MainFrame().ExecuteScript(
          WebScriptSource("const t = document.getElementById('target');"
                          "t.style.transitionProperty = 'none';"
                          "getComputedStyle(t).opacity;"));
    } else {
      MainFrame().ExecuteScript(
          WebScriptSource("const t = document.getElementById('target');"
                          "t.style.opacity = '0.5';"
                          "getComputedStyle(t).opacity;"));
    }
    EXPECT_TRUE(Compositor().NeedsBeginFrame());

    Compositor().BeginFrame(0.016);
    test::RunPendingTasks();
    if (Compositor().NeedsBeginFrame()) {
      Compositor().BeginFrame(0.016);
      test::RunPendingTasks();
    }

    MainFrame().ExecuteScript(WebScriptSource("window.report();"));
    const String expected =
        "resolved=1\n"
        "rejected=\n"
        "transitionend=1\n"
        "transitioncancel=0\n"
        "finish=1\n"
        "cancel=0\n"
        "playState=finished";
    EXPECT_EQ(expected, GetElementById("result")->textContent());
  }
}

}  // namespace blink
