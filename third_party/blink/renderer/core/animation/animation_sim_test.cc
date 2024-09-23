// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/exception_state_matchers.h"

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

  Element* target = GetDocument().getElementById(AtomicString("target"));

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

}  // namespace blink
