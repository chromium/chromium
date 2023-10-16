# core/animation

This directory contains the main thread animation engine. This implements the
Web Animations timing model that drives CSS Animations, Transitions and exposes
the Web Animations API (e.g. `element.animate()`) to Javascript.

[TOC]

## Contacts

* [Animations OWNERS](
https://cs.chromium.org/search?&q=file:blink/renderer/core/animation/OWNERS)
* [Chromium #animations channel on Slack](
https://chromium.slack.com#animations)

## Specifications implemented

*   [CSS Animations Level 1](https://drafts.csswg.org/css-animations-1/)
*   [CSS Animations Level 2](https://drafts.csswg.org/css-animations-2/)
*   [CSS Transitions Level 1](https://drafts.csswg.org/css-transitions-1/)
*   [CSS Transitions Level 2](https://drafts.csswg.org/css-transitions-2/)
*   [Web Animations Level 1](https://w3.org/TR/web-animations-1/)
*   [Web Animations Level 2](https://drafts.csswg.org/web-animations-2/)
*   [Scroll Animations Level 1](https://drafts.csswg.org/scroll-animations-1/)
*   [CSS Properties and Values API Level 1 - Animation Behavior of Custom Properties](
https://www.w3.org/TR/css-properties-values-api-1/#animation-behavior-of-custom-properties)
*   Individual CSS property animation behaviour [e.g. transform interolation](
https://www.w3.org/TR/css-transforms-1/#interpolation-of-transforms).

## Timeline Time

Each animation is linked to a timeline, which measures the passage of time and
is used in determining the current time of the animation. All timelines
implement the [AnimationTimeline interface][]. There are presently two types of
timelines: document timeline, and scroll timeline. The timeline may also be
unresolved. See the section titled "Timeline Details" for a full description
of the timeline attributes. This section highlights a key difference between
timeline types when it comes to the handling of time, which in turn should make
it easier to follow the animations discussion below.

### Document Timeline

Document timelines measure the passage of time in milliseconds relative to an
origin time. Whenever script is executed, a timeline time is computed based on
the animation clock. The timeline time is held constant during script
execution. The clock is updated when a new animation frame is produced.

For a running animation, the link between the timeline time and the
animation's current time is as follows:

animation current time = (timeline time - animation start time) * playback rate

For a document timeline, these times are internally represented as
[AnimationTimeDelta][], and externally represented as optional doubles with
implicit units of milliseconds.

[AnimationTimeDelta]: https://cs.chromium.org/search?&q=class:blink::AnimationTimeDelta$

### Scroll Timeline

Scroll timelines are progress based and measure the passage of time as a
percentage. Each scroll timeline has an effective range, which by default is
the full scroll range; however, it may be set to a smaller range of scroll
offsets. Scroll time is computed as follows:

scroll time = (scroll position - effective start) /
              (effective end - effective start) * 100%

The value is clamped between 0 and 100%. The same equation holds between
timeline time and animation current time. Also like document timelines, times
are represented internally as [AnimationTimeDelta][]. Unlike document
timelines, times are externally represented as [CSSNumericValue][] with
explicit units of percent. The consistent internal treatment of time is
maintained by using a fixed constant value for the conversion between percent
and time. Internally, each progress-based animation, is normalized to 100s.
Through most of the animation discussion below, we will use time calculations in
milliseconds. It is important to remember that unit conversion is required in
the case of a progress-based animation, when reporting or consuming values via
JavaScript APIs.

[CSSNumericValue]: http://drafts.css-houdini.org/css-typed-om/#numeric-value

### Null (Unresolved) Timeline

The timeline may be unresolved, by explicitly setting it to null for an
animation. Note that this is not equivalent to using the default timeline. In
the later case, the timeline is initialized based on the document timeline
associated with the animated element's document  (document fragment when inside
shadowDOM). In practice, a null timeline should be rarely encountered; however,
it is important to keep in mind, since failure to track such edge cases can
easily lead to crashes in the code.

## Types of animations

There are 3 main types of animations that are supported in this directory
subtree: CSS animations, CSS transitions, and web animations. These animation
types are largely differentiated by their creation mechanism, but also have
differences in behavior such as rules for composite ordering. Nonetheless, all
three animation types share a common base class ([Animation](
https://cs.chromium.org/search/?q=class:blink::Animation$)) with the majority
of the code being common to all three animation types.

### CSS animation

CSS animations are created via CSS rules, whereby an animation-name property
(or animation shorthand property) refers to one or more keyframes rules.
Keyframes specify property values at various positions along the animation
curve. For example:

```css
@keyframes fade {
  0% { opacity: 1; }
  100% { opacity: 0; }
}
@keyframes redshift {
  to { color: red; }
}
.fade-out {
  animation: fade 1s linear, redshift 400ms ease-in-out;
}
```

The labels 'from' and 'to' are aliases for '0%' and '100%', respectively.
Keyframes may specify different property values, and the set of keyframes is not
required to specify values at 0% or 100% (partial keyframes). If missing
starting or ending values for a property, a neutral property values is used
based on the underlying value. In the above example, the redshift animation will
animate from the current color to red.

This example illustrates just a few of the options for generating keyframes.
A more detailed explanation can be found on the [MDN @keyframes](
https://developer.mozilla.org/en-US/docs/Web/CSS/@keyframes) page. A more
detailed explanation of the animation shorthand property and its longhand
counterparts can be found on the [MDN > CSS > animation](
https://developer.mozilla.org/en-US/docs/Web/CSS/animation) page.

[CSS Animations](https://cs.chromium.org/search?&q=class:blink::CSSAnimation$)
are created during style update in a multi-stage process. Keyframe models for
pending animations are created in [CalculateAnimationUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateAnimationUpdate
), which in turn calls [CreateKeyframeEffectModel](
https://cs.chromium.org/search?lang=cc&q=function:CreateKeyframeEffectModel%20file:css_animations.cc
). The algorithm for constructing keyframes for a CSS animation is outlined in
[css-animations-2/#keyframes](https://drafts.csswg.org/css-animations-2/#keyframes).
New animations are constructed for the pending animations in
[MaybeApplyPendingUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::MaybeApplyPendingUpdate
). This method also handles updating the timing model of existing CSS animations
or canceling CSS animations.

Changes to the [phase](
https://www.w3.org/TR/web-animations-1/#animation-effect-phases-and-states)
 of a CSS animation may result in firing one or more [AnimationEvent](
https://developer.mozilla.org/en-US/docs/Web/API/AnimationEvent)s. Each CSS
animation has an associated [AnimationEventDelegate](
https://cs.chromium.org/search?lang=cc&q=class:AnimationEventDelegate) with an
[OnEventCondition](
https://cs.chromium.org/search?lang=cc&q=function:AnimationEventDelegate::OnEventCondition) method,
which handles dispatching of these events.

The [CSSAnimation interface](
https://cs.chromium.org/search?q=file:css_animation.idl) extends the
[Animation interface](https://cs.chromium.org/search?q=file:animation.idl) to
include an animationName property, which indicated the name of the keyframes
rule associated with the animation. Note that this association may be broken
by interacting with the animation via the web-animation API.


### CSS transition

Like CSS animations, CSS transitions are triggered by style rules in CSS;
however, unlike CSS animations, the keyframes are inferred. For example:

```css
button {
  background: blue;
  transition: background-color 150ms ease-in;
}

button:hover {
  background: #05f
}
```

In this example, hovering over a button changes the background color. A
transition animation is created from the previous background color to the new
background color. A more detailed description of the transition property and its
longhand counterparts can be found on the [MDN > CSS > Transitions](
https://developer.mozilla.org/en-US/docs/Web/CSS/transition) page.

[CSS Transitions](https://cs.chromium.org/search?&q=class:blink::CSSTransition$)
are created during style update in a multi-stage process. Keyframe models for
pending animations are created in [CalculateTransitionUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateTransitionUpdate$), which
iterates over names in transition-property calling
[CalculateTransitionUpdateForProperty](https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateTransitionUpdateForProperty)
for each property. If there is already an active transition for the property
and the value of the property changes, the transition is retargeted to the new
end point. When retargeting an animation, the current position is used as a
starting point, which is calculated by applying active animations with an
updated timestamp to the underlying style in [CalculateBeforeChangeStyle](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::CalculateBeforeChangeStyle
). The before-change style is lazy evaluated to avoid unnecessary work when no
transition updates are required, and is computed at most once per element per
non-animation style update. New transitions are constructed for the pending
transitions in [MaybeApplyPendingUpdate](
https://cs.chromium.org/search?lang=cc&q=function:CSSAnimations::MaybeApplyPendingUpdate
). This method handles constructing new transitions, retargeting active
transitions, and canceling finished transitions.

Changes to the [phase](
https://www.w3.org/TR/web-animations-1/#animation-effect-phases-and-states)
of a CSS transition may result in firing one or more [TransitionEvent](
https://developer.mozilla.org/en-US/docs/Web/API/TransitionEvent)s. Each CSS
transition has an associated [TransitionEventDelegate](
https://cs.chromium.org/search?lang=cc&q=class:TransitionEventDelegate) with an
[OnEventCondition](
https://cs.chromium.org/search?lang=cc&q=function:TransitionEventDelegate::OnEventCondition) method,
which handles dispatching of these events.

The [CSSTransition interface](
https://cs.chromium.org/search?q=file:css_transition.idl) extends the
[Animation interface](https://cs.chromium.org/search?q=file:animation.idl) to
include an transitionProperty property, which indicated the name of the property
being transitioned. Note that this association may be broken by interacting with
the transition via the web-animation API.

### Web Animation

Web animations are programmatically created in JavaScript via calls to
[Element.animate](
https://cs.chromium.org/search?q=function:Animatable::Animate). Similar to
CSS animations, web-animations require a set of keyframes; however, the
representation of the keyframes is expressed as a list or object in JavaScript.
For example:

```javascript
const slideAnimation = element.animate(
  [ { offset: 1, transform: 'translateX(200px)', easing: 'linear' } ],
  { duration: 1000,  easing: linear });
const gravityAnimation = element.animate(
  { transform: ['none', 'translateY(400px' ] },
  { duration: 1000, easing: 'ease-in', composite: 'add' });
```

This example illustrates two formats for expressing the list of keyframes.
The keyframes argument to animate may be a list of keyframes with optional
offsets. If offsets are not specified, then equal spacing is assumed. The first
of the two animations illustrates the use of partial keyframes. For this
animation, a neutral keyframe is inferred at offset 0.

If animating a single property, they keyframes may be expressed as a object with
the property name as a key and a list of string/numeric values for the property.
In this case, equally spaced keyframes are implied. This example also
illustrates the use of composite modes to combine multiple effects on a single
property. Since the easing functions are different for the two animations, they
cannot be combined into a single animation to achieve the desired results.

A more detailed description of the animate method can be found on the
[MDN Element.animate](
https://developer.mozilla.org/en-US/docs/Web/API/Element/animate) page.

## Web animation model

At a fundamental level, the web animation model converts a time value to one or
more property values. This is true whether we are using a DocumentTimeline
driven by an AnimationClock, or a ScrollTimeline that converts a scroll position
to an abstract representation of time. The same rules also apply for
CSSAnimations and CSSTransitions, which derive from the base Animation class.

The web animation model can be further broken down into two sub-models:

1. A 'timing model' which converts time to an iteration index and progress
(proportional value between 0 and 1).

2. An 'animation model' which converts the progress to property values.

![](https://www.w3.org/TR/web-animations-1/images/timing-and-animation-models.svg)

The division of responsibilities can best be illustrated through an example.
Consider the following:

```css
@keyframes fade {
  from { opacity: 1; animation-timing-function: ease-in; }
  50% { opacity: 0.5; animation-timing-function: ease-out; }
  to { opacity: 0; }
}
.pulse {
  animation-name: fade;
  animation-iteration-count: 3;
  animation-duration: 0.5s;
  animation-delay: 200ms;
  animation-direction: alternate-reverse;
  animation-fill: both;
}
```
```javascript
element.classList.add('pulse');
```

Prior to the 200ms mark, the animation is in the 'before' phase. since the
elapsed time is less than the start delay (200ms). Conversely, after the 1.7s
mark (start delay + itertions * duration), the animation is in the 'after'
phase. Between these time boundaries, we are in the 'active' phase, and the
timing model is applied to calculate the iteration index and progress.

At the 1s mark of the animation, we are 0.8s into playing the animation (active
time) due to the start delay. Each iteration takes 0.5s, putting us 0.3s into
the second iteration, which is in the forward direction since alternating and
initially playing backwards (alternate-reverse). The last step of the timing
model is to convert the iteration time to an iteration progress. This is simply
the ratio of the iteration time to the iteration duration (0.3s / 0.5s = 0.6).

Conversely, at the 0.5s mark, we are 0.3s into an iteration playing in the
reverse direction. As the iteration progress measures progress in the forward
direction, the iteration progress becomes 1 - directional progress, which is
1 - 0.3s / 0.5s = 0.4.

The animation model converts the iteration index and progress into property
values. As the iteration-composite property is not supported at this time, only
the iteration progress is a factor in our calculations. The iteration progress
is fed into the keyframes model to compute property values.

Each keyframe has an offset, and for each property, we determine the keyframe
pair that bounds the iteration progress. Returning to our example with an
iteration progress of 0.6 at the 1s mark, we are iterating between the '50%'
and 'to' keyframes. The relative progress between these frames is
(0.6 - 0.5) / (1.0 - 0.5) = 0.2. This value is the input to our
animation-timing-function. The timing function for this keyframe pair
is 'ease-out', which is equivalent to cubic-bezier(0, 0, 0.58, 1)). Plugging an
input value of 0.2 into our cubic-bezier function, we get an output of roughly
0.31, which is the local progress value used for interpolation between the two
keyframes. For any scalar-valued property, the interpolation function is:

v = progress * v_1 + (1 - progress) * v_0

For our example,

opacity = 0.31 * 0 + (1 - 0.31) * 0.5 = 0.35

The interpolation procedure is less straightforward for non-scalar values
(especially for transform lists). Nonetheless, this example provides a good
overview of the web animation model.

Points of interest in the Blink code base:
* [timing_calculations.h](
https://cs.chromium.org/search?q=file:core/animation/timing_calculations.h):
contains static helper functions for various timing calculations used in the
animation model such as determining the phase of the animation, iteration
progress and transformed progress.
* [AnimationEffect::UpdateInheritedTime](
https://cs.chromium.org/search?q=function:blink::AnimationEffect::UpdateInheritedTime):
applies the web animation model and dispatches animation/transition events.
* [InterpolationEffect::GetActiveInterpolations](
https://cs.chromium.org/search?q=function:blink::InterpolationEffect::GetActiveInterpolations):
creates a list of active interpolations by determining keyframe-pairs that bound
the iteration progress and determining the 'local' progress between keyframes.
If a timing function is specified for the interpolation, it is applied when
computing the 'local' progress.
* [KeyframeEffect::ApplyEffects](
https://cs.chromium.org/search?q=function:blink::KeyframeEffect::ApplyEffects):
Samples the keyframe effect, adding it to the effect stack if necessary and
signaling that an animation style recalc is needed if the sampled value changed.
* [Animation::Update](
https://cs.chromium.org/search?q=function:blink::Animation::Update):
Called in response to ticking the animation timeline or to revalidate outdated
animations. In addition to applying the web animation model via
UpdateInheritedTime, this method determines if the animation is finished.


## Web-animation API

The Web-animation API provides a unified framework for interacting with
animations regardless of the animation type (creation mechanism). CSS
animations and transitions can also be manipulated via the API even though not
created via the Element.animate method. The following sections cover each of
the JavaScript extensions that build up the web-animation API.

### Animatable

Objects that may be the target of a KeyframeEffect implement the [Animatable][]
interface. At present, this set of objects is restricted to Element and
derived classes. Refer to the web-animation section under animation types for a
description of Element.animate.

The animatable interface also includes a getAnimations method, which returns all
active animations associated with the element in composite ordering. The rules
for composite ordering are quite involved and presently span three
specifications:

* [web animations -- the effect stack](
https://w3.org/TR/web-animations-1/#the-effect-stack
)
* [css animations -- animation composite order](
https://drafts.csswg.org/css-animations-2/#animation-composite-order
)
* [css transitions -- animation composite order](
https://drafts.csswg.org/css-transitions-2/#animation-composite-order
)

Style updates for CSS transitions, are applied before updates for CSS
animations, which in turn are applied before generic web animations. Each
effect, when applied, can replace or combine with the previous effects in the
effect stack depending on the composite mode (refer to discussion of composite
modes in the KeyframeEffect section). Additional rules apply for sorting CSS
transitions and CSS animations.

[Animatable]: https://w3.org/TR/web-animations-1/#the-animatable-interface-mixin

### Animation

The [Animation interface][] contains methods and attributes for programmatically
interacting with animations. [Animation object][]s may be created via the
constructor or the Element.animate method covered previously.
[Animation object][]s may be queried via a getAnimations call. The query
retrieves all animations regardless of type and may be used to interact with CSS
animations or transitions.

The animation API consists of the following attributes and methods:

* **[Animation constructor][]**: This constructor is called regardless of the
type of animation. The constructor may also be called directly from JavaScript
to create a web animation, which is particularly useful when a [timeline][]
other than the [default document timeline][] is being used.

   ```javascript
   const keyframeEffect = new KeyframeEffect(...);
   const timeline = new ScrollTimeline(...);
   const animation = new Animation(keyframeEffect, timeline);
   // Unlike Element.animate, an animation created directly via the constructor
   // does not autoplay.
   animation.play();
   ```

* **[id attribute][]**: sets or gets an id that can be used to identify the
animation. These ids are purely informational, but may be used by the developer
to facilitate bookkeeping or debugging.

* **[effect attribute][]**: gets or sets the [AnimationEffect][] for the
animation. The algorithm for setting the effect of an animation is outlined in
[web-animations -- Setting the associated effect of an animation](
https://w3.org/TR/web-animations-1/#setting-the-associated-effect). A few
extra steps are required for CSS animations and transitions, which have an
[AnimationEffect::EventDelegate][] that need to be reattached after the effect
is updated. Special handling is also required if the new effect is null to
cancel or finish the CSS animation / transition.

   ```javascript
   // Clone an animation effect.
   const animationA = elementA.animate(...);
   const animationB = elementB.animate(animationA.effect.getKeyframes(),
                                       animationA.effect.getTiming());
   ```

* **[readonly timeline attribute][]**: At present Blink only supports fetching
the associated timeline. Efforts are underway to support setting the timeline as
well, which is useful for attaching a scroll timeline to an animation created
via Element.animate. Refer to the [Timelines][] section of the web animation
spec for more details.

   ```javascript
   const animation = element.animate(...);
   assert_equals(animation.timeline, document.timeline);
   ```

* **[startTime attribute][]**: gets or sets the start time of an animation. The
start time is unresolved when the animation is paused, idle or play-pending
(scheduled to play but not yet acked by the client). A running (if not
play-pending) or finished animation has a resolved start time. The current time
of an animation is determined from the timeline time, start time and playback
rate if the animation is not paused or finished. Setting the start time of an
animation is applied immediately rather than waiting on an ack from the client;
however, a resulting change in the finished state will not resolve a finished
promise or queue an onfinish handler until the next [microtask checkpoint][]
since the state change may be temporary. The setStartTime method is overridden
for CSSAnimations since calling it may update the animation-play-state property
by unpausing a CSS animation. Refer to
[Web-animations -- Setting the start time of an animation](
https://w3.org/TR/web-animations-1/#setting-the-start-time-of-an-animation)
for more detail.

   ```javascript
   const animation = element.animate(...);
   // The newly created animation is scheduled to play, but the start time is
   // unresolved until acked by the client agent.
   assert_true(animation.pending);
   assert_equals(animation.playState, 'running');
   assert_false(!!animation.startTime);
   animation.ready.then(() => {
     assert_false(animation.pending);
     assert_times_equal(animation.startTime, document.timeline.currentTime);
   });
   ```

   ```javascript
   const animation = element.animate(...);
   aniamtion.pause();
   // The animation is in a pending-paused state.
   assert_true(animation.pending);
   assert_equals(animation.playState, 'paused');
   assert_false(!!animation.startTime);
   assert_equals(animation.currentTime, 0);
   // Explicitly setting the start time aborts the pending-pause, and starts the
   // animation.
   animation.startTime = document.timeline.currentTime;
   assert_false(animation.pending);
   assert_equals(animation.playState, 'running');
   assert_times_equal(animation.startTime, document.timeline.currentTime);
   assert_equals(animation.currentTime, 0);
   ```

* **[currentTime attribute][]**: gets or sets the current time of an animation.
When setting the current time, either the start time or hold time of the
animation is updated depending on the play state. The start time is updated for
a running or finished animation, and the hold time is updated for an idle or
paused animation. The change is applied immediately and does not wait on an ack
from the client. Similar to setting the start time, updating the current time
will not resolve a finished promise or queue an onfinished event until the next
[microtask checkpoint][] since the state change may be temporary. Refer to
[Web-animations -- The current time of an animation](
https://w3.org/TR/web-animations-1/#the-current-time-of-an-animation) and
[Web-animations -- Setting the current time of an animation](
https://w3.org/TR/web-animations-1/#setting-the-current-time-of-an-animation)
for more detail.

   ```javascript
   const animation = element.animate(...);
   // Advance to 1000ms mark in the animation. Since the animation is
   // play-pending the hold time is updated. Once ready, the start time will
   // be set to align with the hold time.
   animation.currentTime = 1000;
   assert_true(animation.pending);
   assert_equals(animation.playState, 'running');
   animation.ready.then(() => {
     assert_false(animation.pending);
     assert_times_equal(animation.startTime,
                        document.timeline.currentTime - 1000);
     assert_times_equals(animation.currentTime, 10000);
   });
   ```

* **[playbackRate attribute][]**: gets or sets the playback rate of the
animation. An animation with a negative value for the playback rate plays in the
reverse direction. The setter is closely related to the updatePlaybackRate
method with the difference that with this setter the change takes place
immediately and does not require an ack from the client. The getter reports the
active playback rate and not the pending playback rate. These values may differ
for a brief time interval if using updatePlaybackRate to asynchronously change
the playback rate. The algorithm for setting the playback rate is covered under
[web animations -- setting the playback rate on an animation](
https://w3.org/TR/web-animations-1/#setting-the-playback-rate-of-an-animation)
in the spec. An additional step in required in Blink to ensure that a composited
animation is kept in sync with a change to the playback rate and to prevent a
discontinuity (jump) in the animation. Typically, using updatePlaybackRate is
preferable to using the setter.

   ```javascript
   const animation = element.animate(...);
   assert_equals(animation.playbackRate, 1);
   animation.playbackRate = -1;
   // The change to the playback rate is reflected immediately.
   assert_equals(animation.playbackRate, -1);
   ```

* **[readonly playState attribute][]**: gets the play state of an animation,
which may be one of the following: 'idle', 'paused', 'running', or 'finished'.
At present, the play state in Blink has an extra state called 'pending', which
is not used in web animations but is still externally referenced. The play state
is forward looking insofar as a pending-play animation will report running and a
pending-pause animation will report paused. The pending attribute can be checked
to disambiguate whether the reported play state reflects the current or the
scheduled state. The algorithm for determining the play state is outlined in
[web animations -- play states](
https://w3.org/TR/web-animations-1/#play-states).

   ```javascript
   const animation = element.animate(...);
   // play state is updated even though the animation has not ticked.
   assert_equals(animation.playState, 'running');
   animation.pause();
   assert_equals(animation.playState, 'paused');
   ```

* **[readonly replaceState attribute][]**: gets the replace state of an
animation, which may be one of the following: 'active', 'removed', or
'persisted'. A replaceState of 'active' indicates that the animation is in
effect, but may be replaced if conditions are satisfied (finished and all
affected properties are also affected by other finished animations higher in
composite order). A 'removed' animation is an animation that is finished and
marked for removal due to being replaceable. A persisted animation is an
animation that has been explicitly marked for exclusion for the automated
removal process via the persist method. The procedure for marking and removing
animations is covered in [web animations - replacing animations](
https://w3.org/TR/web-animations-1/#replacing-animations). In the Blink
implementation, identifying which animations are replaceable is done in
[Animation::IsReplaceable][]. Removal of replaced animations is done in
[AnimationTimeline::RemoveReplacedAnimations][], which calls
[Animation::RemoveReplacedAnimation][]. Removal is done during the timeline
update cycle after animations have ticked and their finished states have been
updated.

   ```javascript
   function commitPersistedAnimations() {
     document.getAnimations().forEach((anim) => {
       if (anim.playState == ‘finished’ &&
           anim.replaceState == ‘persisted’) {
         anim.commitStyles();
         anim.cancel();
       }
     });
   };
   ```

* **[readonly pending attribute][]**: gets the pending status of an animation.
Changes to the play state via the play, pause, and reverse methods do not take
effect immediately, but instead schedule a task to execute once the
animation is ready (acked by the client agent). An animation in the 'running'
playState is pending until it receives a start time. Conversely, a
pending-pause animation may still have a start-time until the client agent is
ready to pause the animation. As the play state of a CSS animation may also be
changed via the animation-play-state property, a style flush is required when
querying the play state of a CSS animation.

   ```javascript
   div.style.animation = `fade 1s linear`;
   const animation = div.getAnimations[0];
   assert_equals(animation.playState, 'running');
   assert_true(animation.pending);
   animation.ready.then(() => {
     assert_false(animation.pending);
     // Updating the play state via the animation-play-state property causes
     // the aniamtion to be pause-pending.
     div.style.animationPlayState = 'paused';
     assert_equals(animation.playState, 'paused');
     assert_true(animation.pending);
     animation.ready.then(() => {
       assert_false(animation.pending);
     });
   });
   ````

* **[readonly ready attribute][]**: The ready promise is resolved when
pending play or paused operations are acked by the client agent. A change that
requires an ack from the client agent must call
[Animation::SetCompositorPending][]. The method name is somewhat misleading as
it applies whether or not the animation is actually run on the compositor.
The method updates the list of pending animations if required. In turn,
[PendingAnimations::Update][] updates the list of animations that are waiting
for a start time, and notifies that the animation is ready if not requiring a
start time. If all animations needing a start time are main-thread animations,
they are also marked as ready. If at least one animation is composited, all new
animations created during the update cycle must wait on the compositor in order
to properly synchronize the start times. Animations from a previous cycle are
exempt from start synchronization to guard against plugging up of the animation
pipeline. Composited animations call
[PendingAnimations::NotifyCompositorAnimationStarted][] with the start time,
which in turn calls [Animation::NotifyReady][]. If all animations for an element
are running on the main thread, then NotifyCompositorAnimationStarted is called
directly. Again, the method name is somewhat misleading.

   ```javascript
   const slideAnimation = element.animate(...);
   const fadeAnimation = element.animate(...);
   slideAnimation.ready.then(() => {
     // Synchronize the fade animation to run 1s after the start of the slide
     // animations.
     fadeAnimation.startTime = slideAnimation + 1000;
   })
   ```

* **[readonly finished attribute][]**:  The finished promise is resolved after
an animation finishes. Since the finished state may be temporary, the resolution
is deferred until the next [microtask checkpoint][]. This delay facilitates
getting consistent results when the ordering of API calls is changed. For
example, setting the current time to the end time and reversing the direction of
the animation should not resolve the finished promise. API calls that affect
current time or play state must update the finished state of the animation. The
algorithm is outlined in [web animation -- updating the finished state](
https://w3.org/TR/web-animations-1/#updating-the-finished-state). In
Blink, updating the finished state and scheduling the microtask is performed in
[Animation::UpdateFinishedState][]. The microtask is handled in
[Animation::AsyncFinishMicrotask][]. The finished promise is convenient for
chaining together animations and for applying style updates.

   ```javascript
     const animation = element.animate(keyframes,
                                       { duration: 1000,
                                         fill: `forwards`
                                       });
     animation.finished.then(() => {
       animation.commitStyles();
       animation.cancel();
       // Follow up animation.
       element.animate(...);
     });
   ```

* **[onfinished attribute][]**: gets or sets the onfinished event handler that
allows the web developer to attach customized JavaScript to run once an
animation has finished.

   ```javascript
   const animation = element.animate(keyframes,
                                     {
                                       duration: 1000
                                       fill: 'forwards'
                                     });
   animation`.onfinished = () => {
     animation.commitStyles();
     animation.cancel();
   };
   ```

* **[oncancel attribute][]**: gets or sets the oncancel event handler that
allows the web developer to attach customized JavaScript to run once an
animation has been canceled.

   ```javascript
   const animation = element.animate(keyframes,
                                     {
                                       duration: 1000
                                       fill: 'forwards'
                                     });
   animation.oncancel = () => {
     myTrackedAnimations.remove(animation);
   };
   ```

* **[onremove attribute][]**: gets or sets the onremove event handler that
allows the web developer to attach customized JavaScript to run once an
animation has been removed.

   ```javascript
   const animation = element.animate(keyframes,
                                     {
                                       duration: 1000
                                       fill: 'forwards'
                                     });
   animation.onremove = () => {
     animation.commitStyles();
   };
   ```

* **[cancel method]**: synchronously cancels an animation. This operation will
reject any pending read or finished promise, cancel any pending play or pause
task, and queue a cancel event.

   ```javascript
   const animation = element.animate(keyframes,
                                     { duration: 1000, fill: 'forwards' });
   animation.finished.then(() => {
     assert_unreached();
   });
   animation.ready.then(() => {
     assert_unreached();
   });
   animation.currentTime = 500;
   animation.cancel();
   assert_equals(animation.currentTime, null);
   assert_equals(animation.playState, 'idle');
   assert_false(animation.pending);
   ```

* **[finish method]**: synchronously updates the current time of the animation
to be the end time. If playing in the revsere direction, the current time will
be set to zero. The finished promise is immediately resolved and finish event
queued.

   ```javascript
   const animation = element.animate(keyframes,
                                     { duration: 1000, fill: 'forwards' });
   animation.finish();
   assert_equals(animation.currentTime, 1000);
   assert_equals(animation.playState, 'finished');
   assert_false(animation.pending);
   ```

* **[play method]**: schedules an animation to begin playing. The animation
will resume playing from the hold time. If the hold time is unresolved at the
time play is called, it will be set to the start or end time depending on the
direction of the pending playback rate. The initialization procedure is
altered slightly if a scroll timeline is used instead of a document timeline.
In this case, the start time is set directly. Nonetheless, an ack is still
required from the client agent. [Animation::NotifyReady][] calls
[Animation::CommitPendingPlay][] to run the microtask for syncing the start
time, resetting the hold time and resolving the ready promise.

   ```javascript
   const animation = new Animation(kefyrameEffect, document.timeline);
   animation.play();
   ```

* **[pause method]**: schedules an animation to pause. The pending state remains
true until acked from the client agent. [Animation::NotifyReady][] calls
[Animation::CommitPendingPause][] to run the microtask for updating the hold
time, resetting the start time, and resolving the ready promise.

   ```javascript
   const animation = element.animate(keyframes,
                                     { duration: 1000, fill: 'forwards' });
   button.onclick = () {
     if (animation.playState == 'running')
       animation.pause();
     else
       animation.play();
   }
   ```

* **[updatePlaybackRate method]**: sets a pending playback rate for the
animation. The change does not take effect until acked by the client agent.
[Animation::NotifyReady][] calls [Animation::CommitPendingPlay][] or
 [Animation::CommitPendingPause][] depending on the play state. The algorithm
 is specced in [web animations -- seamlessly updating the playback rate...](
 https://w3.org/TR/web-animations-1/#seamlessly-updating-the-playback-rate-of-an-animation).

   ```javascript
   const animation = element.animate(...);
   // The playback rate is updated even though the animation has not ticked.
   assert_equals(animation.playbackRate, 1);
   aniamtion.playbackRate = -1;
   assert_equals(animation.playbackRate, -1);
   animation.updatePlayback(-2);
   // The playback rate is not updated until acked by the client agent.
   assert_equals(animation.playbackRate, -1);
   assert_true(animation.pending);
   animation.ready.then(() => {
     assert_equals(animation.playbackRate, -2);
     assert_false(animation.pending);
   });
   ```

* **[reverse method]**: reverses the direction of a running animation. The
change to playback rate does not take effect until acked by the client agent.
An animation that is not running is started in the reverse direction.

   ```javascript
   const animation = element.animate(keyframes, { duration: 1000 });
   animation.reverse();
   assert_true(animation.pending);
   // Change to playback rate has not yet taken effect.
   assert_equals(animation.playbackRate, 1);
   animation.ready.then(() => {
      // Snap to the end of the animation when playing in the reverse direction.
      assert_times_equal(animation.currentTime, 1000);
      assert_equals(aniamtion.playbackRate(1));
      assert_false(animation.pending);
   });
   ```

* **[persist method]**: marks an animation as persistent such that it is exempt
from being marked and removed as a replaceable animation.

   ```javascript
   const animationA = element.animation({ transform: ['scale(1)', 'scale(2)'] },
                                        { duration: 1000, fill: 'forwards' });
   const animationB = element.animation({ transform: ['scale(1)', 'scale(2)'] },
                                        { duration: 1000,
                                          composite: 'accumulate',
                                          fill: 'forwards' });
   animationB.finished.then(() => {
     // The first animation will be automatically be removed since the second
     // animation is higher in the composite order and affects the same
     // property. Removal of the first animation will result in a visual change
     // since using composite mode 'accumulate' for the second animation. We
     // can prevent the removal by persisting the first animation.
     assert_equals(animationA.removeState, 'removed');
     aniamtionA.persist();
     // The first animation is once again active.
     assert_equals(aniamtionA.removeState, 'persist');
   });
   ```

* **[commitStyles method]**: adds an inline style to the target element based on
the current value of the effect stack up to an including the animation.

   ```javascript
   const animationA = element.animation({ transform: ['scale(1)', 'scale(2)'] },
                                        { duration: 1000, fill: 'forwards' });
   const animationB = element.animation({ transform: ['scale(1)', 'scale(2)'] },
                                        { duration: 1000,
                                          composite: 'accumulate',
                                          fill: 'forwards' });
   animationA.finished.then(() => {
     // The first animation will be automatically be removed since the second
     // animation is higher in the composite order and affects the same
     // property. Removal of the first animation will result in a visual change
     // since using composite mode 'accumulate' for the second animation.
     // Rather than persisting the first animation, we can instead call
     // commitStyles to capture the current state of the animation effect stack
     // in an inline style.
     animationA.commitStyles();
     // element.style is now set to capture the finished state of the first
     // animation, and it can be safely removed without introducing a visual
     // change.
     animationA.cancel();
   });
   ```

[Animation interface]: https://w3.org/TR/web-animations-1/#the-animation-interface
[Animation object]: https://cs.chromium.org/search/?q=class:blink::Animation$
[timeline]: https://w3.org/TR/web-animations-1/#timelines
[Timelines]: https://w3.org/TR/web-animations-1/#timelines
[default document timeline]: https://w3.org/TR/web-animations-1/#the-documents-default-timeline
[AnimationEffect::EventDelegate]: https://cs.chromium.org/search?lang=cc&q=class:AnimationEffect::EventDelegate$
[Animation constructor]:  https://cs.chromium.org/search/?q=function:blink::Animation::Animation$
[id attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::(setI|i)d$
[effect attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::(setE|e)ffect$
[readonly timeline attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::timeline$
[startTime attribute]: https://cs.chromium.org/search/?q=function:blink::(CSS|)Animation::(setS|s)tartTime$
[currentTime attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::(setC|c)urrentTime$
[playbackRate attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::(setP|p)laybackRate$
[readonly playState attribute]:  https://cs.chromium.org/search/?q=function:blink::Animation::playState$
[readonly replaceState attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::replaceState$
[readonly pending attribute]: https://cs.chromium.org/search/?q=function:blink::(CSS|)Animation::pending$
[readonly ready attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::ready$
[readonly finished attribute]: https://cs.chromium.org/search/?q=function:blink::Animation::finished$
[onfinished attribute]: https://cs.chromium.org/search/?q=file:animation.h+DEFINE_ATTRIBUTE_EVENT_LISTENER
[oncancel attribute]: https://cs.chromium.org/search/?q=file:animation.h+DEFINE_ATTRIBUTE_EVENT_LISTENER
[onremove attribute]: https://cs.chromium.org/search/?q=file:animation.h+DEFINE_ATTRIBUTE_EVENT_LISTENER
[cancel nethod]: https://cs.chromium.org/search/?q=function:blink::Animation::cancel$
[finish method]: https://cs.chromium.org/search/?q=function:blink::Animation::finish$
[play method]: https://cs.chromium.org/search/?q=function:blink::Animation::play$
[pause nethod]: https://cs.chromium.org/search/?q=function:blink::Animation::pause$
[updatePlaybackRate nethod]: https://cs.chromium.org/search/?q=function:blink::Animation::updatePlaybackRate$
[reverse method]: https://cs.chromium.org/search/?q=function:blink::Animation::reverse$
[persist method]: https://cs.chromium.org/search/?q=function:blink::Animation::persist$
[commitStyles method]: https://cs.chromium.org/search/?q=function:blink::Animation::commitStyles$
[Animation::SetCompositorPending]: https://cs.chromium.org/search/?q=function:blink::Animation::SetCompositorPending$
[PendingAnimations::Update]: https://cs.chromium.org/search/?q=function:blink::PendingAnimations::Update$
[Animation::UpdateFinishedState]: https://cs.chromium.org/search/?q=function:blink::Animation::UpdateFinishedState$
[Animation::AsyncFinishMicrotask]: https://cs.chromium.org/search/?q=function:blink::Animation::AsyncFinishMicrotask$
[PendingAnimations::NotifyCompositorAnimationStarted]: https://cs.chromium.org/search/?q=function:blink::PendingAnimations::NotifyCompositorAnimationStarted$
[Animation::NotifyReady]: https://cs.chromium.org/search/?q=function:blink::Animation::NotifyReady$
[Animation::CommitPendingPlay]: https://cs.chromium.org/search/?q=function:blink::Animation::CommitPendingPlay$
[Animation::CommitPendingPause]: https://cs.chromium.org/search/?q=function:blink::Animation::CommitPendingPause$
[Animation::IsReplaceable]: https://cs.chromium.org/search/?q=function:blink::Animation::IsReplaceable$
[AnimationTimeline::RemoveReplacedAnimations]: https://cs.chromium.org/search/?q=function:blink::AnimationTimeline::RemoveReplacedAnimation$
[Animation::RemoveReplacedAnimation]: https://cs.chromium.org/search/?q=function:blink::Animation::RemoveReplacedAnimation$
[microtask checkpoint]: https://developer.mozilla.org/en-US/docs/Web/API/HTML_DOM_API/Microtask_guide


### AnimationEffect

The [AnimationEffect interface][] contains methods for querying and updating
the timing of an animation. Refer to the discussion on 'timing model' in the
section titled 'web animation model' for a high level overview of animation
timing.

* **[getTiming method][]**: returns an [EffectTiming][] object that contains a
dictionary of optional specified timing parameters. This object has methods of
the form: hasParam(), param(), and setPararm() in C++. In JavaScript, each of
the parameters has a getter/setter pair. If the effect is associated with a CSS
animation, the style is flushed prior to fetching the timing since timimg
parameters may be set via CSS properties. The properties are as follows:

    * **delay**: The start delay of the animation and equivalent to the CSS
      animation property animation-delay. An animation with a negative start
      delay will appear to have started ahead of when it was applied. The delay
      is expressed in milliseconds.
    * **direction**: The direction for playing the animation, which may be one
      of the following: 'forward', 'reverse', 'alternate' or
      'reverse-alternate'. The parameter is equivalent to the CSS animation
      property animation-direction.
    * **duration**: The duration for a single iteration of the animation,
      which may be expressed as a double or a string. If string valued, it
      contains the time unit as part of the value. Otherwise the value is a time
      expressed in milliseconds. The corresponding CSS parameter is
      animation-duration.
    * **easing**: The default timing function of the animation. This timing
      function is used for keyframes if not overridden within the keyframe
      properties. Refer to the sections titled 'CSS animation', 'web
      animation model', and 'keyframeEffect' for more details on keyframes.
      This parameter deviates from the normal naming convention when mapping to
      the CSS property name. The name of the equivalent CSS property is
      animation-timing-function.
    * **endDelay**: The end delay of the animation. There is no CSS property
      counterpart for this parameter. If set, the end delay specifies the
      number of milliseconds between the active phase and the end time (sounds
      ominous).
    * **fill**: The fill mode of an animation, which defines whether the
      animation persists once finished. The fill mode can be one of the
      following: 'none', 'forwards', 'backwards', 'both', or 'auto'.
      An animation with fill 'forwards' will persist after finishing if playing
      in the forward direction. Conversely, an animation with fill 'backwards'
      will persist after finishing if playing in the reverse direction. An
      animation with fill 'both' will persist when playing in either direction.
      The persistence of a fill mode animation may be overruled if the animation
      is marked as replaceable as outlined in the section on the Animation API.
      The corresponding CSS property is animation-fill-mode.
    * **iterationStart**: If set, this parameter indicates where the animation
      starts within the iteration cycle. The animation will still complete the
      number of iterations specified in the 'iterations' parameter, but may
      start and end part way through an iteration cycle. There is no CSS
      property associated with this parameter. Unlike start delay, this
      parameter does not affect the runtime of the animation.
    * **iterations**: Specified the number of iterations to complete in the
      animation. The corresponding CSS property is animation-iteration-count.

* **[getComputedTiming method][]**: returns a [ComputedEffectTiming][] object
that contains a dictionary of computed timing parameters. As
[ComputedEffectTiming][] extends [EffectTiming][], this dictionary contains
 a superset of properties; however, values may differ from [EffectTiming][] due
 to being evaluated. Default values for missing properties are resolved, the
 duration is expressed solely in milliseconds, and a fill mode of 'auto' is
 resolved to fill 'none'. All values needed for the 'timing model' portion of
 the web animation model are included. The follow are the set of additional
 properties:

    * **endTime**: In the absence of a start or end delay, the endTime would be
      the total runtime of the animation (iteration duration * iteration count).
      A positive value for start or end delay extends the runtime of an
      animation; however, only the end delay factors into the calculation of
      endTime. The value for endTime is calculated as the duration of the
      active phase plus the end delay.
    * **activeDuration**: The active duration of the animation is simply the
      product of the iteration duration and iteration count.
    * **localTime**: The value for localTime is Animation.currentTime if
      the effect is associated with an animation, otherwise unresolved.
    * **progress**: Specifies the [transformed progress][]. Steps in calculating
      the progress of an animation are also outlined in the section titled
      'web animation model'.
    * **currentIteration**: Indicates the current (zero-based) index of the
      animation.

* **[updateTiming method][]**: used to update one or more specified timing
properties. Any property updated in this manner for a CSS animation is marked
so that subsequent updates via CSS are ignored. In other words, explicit use
of the web-animation API takes precedence over CSS.

   ```javascript
   target.style.animation = 'spinner 1s infinite linear';
   const animation = target.getAnimations()[0];
   assert_equals(animation.effect.getTiming().duration, 1000);

   // Update via CSS property. Style changed properly flushed when fetching
   // timing properties.
   target.style.animationDuration = '3s';
   assert_equals(animation.effect.getTiming().duration, 3000);

   // Update via web animation API.
   animation.effect.updateTiming( {duration: 2000 });
   assert_equals(animation.effect.getTiming().duration, 2000);

   // Attempted update via CSS property is now ignored.
   target.style.animationDuration = '3s';
   assert_equals(animation.effect.getTiming().duration, 2000);
   ```

[AnimationEffect interface]: https://cs.chromium.org/search/?q=file:core/animation/animation_effect.idl
[getTiming method]: https://cs.chromium.org/search/?q=function:blink::AnimationEffect::getTiming$
[getComputedTiming method]: https://cs.chromium.org/search/?q=function:blink::AnimationEffect::getComputedTiming$
[updateTiming method]: https://cs.chromium.org/search/?q=function:blink::AnimationEffect::updateTiming$
[EffectTiming]: https://cs.chromium.org/search/?q=class:blink::EffectTiming$
[ComputedEffectTiming]: https://cs.chromium.org/search/?q=class:blink::ComputedEffectTiming$
[transformed progress]: https://w3.org/TR/web-animations-1/#calculating-the-transformed-progress


### KeyframeEffect

The [KeyframeEffect interface][] extends the [AnimationEffect interface][]
including attributes and methods specific to keyframes. A brief overview of
keyframes is presented in the section on animation types and in the example for
the web animation model.

* **[KeyframeEffect constructor][]**: creates a new [KeyframeEffect object][].
The constructor takes an element, set of keyframes, and an optional set
[KeyframeEffectOptions][] or a numeric value (duration). All options available
for AnimationEffects may also be used for KeyframeEffects. In addition,
[KeyframeEffectOptions][] contains 'composite' and 'pseudoElement' attributes
that are described below.

* **[target attribute][]**: gets or sets the target element for the animation
effect. If animating a CSS transition or CSS animation, transition/animation
events after a target change are directed back to the original target element.

* **[pseudoElement attribute][]**: gets or sets the pseudo-element specifier for
the target element. For example, an animation triggered by a "div::hover" CSS
rule will have the parent element as the target element and "::hover" as the
pseudoElement specification. The pseudoElement attribute is empty if not
animating a pseudoElement.

* **[composite attribute][]**: gets or sets the composite mode for the animation
effect. The composite mode is not to be confused with compositing (accelerated
rendering off of the main-thread). The composite mode may be one of the
following: 'replace', 'add', 'accumulate', or 'auto'. The default mode is
'replace', where the effect replaces any underlying property value. The 'add'
composite mode combines the effect with the underlying value. For list valued
properties such as transforms, 'add' appends the effect to the list. For
additive properties such as width, the output is the sum of the effect with the
underlying value. The 'accumulate' option also combines the effect with the
underlying value. In the case of a list valued property such transform, the
combination is on a per transform basis, combining scales, translations,
rotations etc. For additive properties such as translate and rotate, the result
is the sum of the effect and underlying value. For multiplicative properties
such as scale, the output is the addition of deltas. A scale(2) operation is a
100% size increase over the base. So 'accumulating' scale(3) on top of scale(2)
= 200% increase + 100% increase = 300% increase = scale(4).

* **[getKeyframes method][]**: retrieves the list of kefyrames. Internally,
Blink maintains two styles of keyframes: 1) [TransitionKeyframes][] (used
solely for CSS transitions), and 2) [StringKeyframes][]. Values stored in
transition keyframes are fairly low-level, being tightly coupled with Blinks
implementation of the interpolation stack. Conversely, string keyframes can
hold any parse-able value, and thus can represent the richness of expressions
available to @keyframes rules. Each KeyframeEffect is associated with a
[KeyframeEffectModel][] that is templated based on the type of keyframe stored.
Keyframes rules for CSS animations cannot be precisely represented in the
dictionary form returned by the getkeyframes call. Various substitutes need to
be made such as resolving variable references, shorthand property values, and
filling in missing keyframe values. These substitutions cannot be made at parse
time when creating the animation since they depend on the style cascade.
Instead, the resolution is done on demand when getKeyframes is called. CSS
animations use a specialized keyframe model ([CssKeyframeEffectModel][]) which
overrides [getComputedKeyframes][] to perform the necessary resolution.

* **[setKeyframes method][]**: replaces the keyframes associated with the
effect. The effect is updated to use a StringKefyrameModel (i.e.
KeyframeEffectModel<StringKeyframe>) regardless of the original format for the
keyframes. Note that CSS transitions can be updated to animate properties
other than the one specified in CssTransition.transitionProperty. In the event
of transition retargeting, the transition's current time is used for computing
the current progress even if no longer transitioning the property.

[KeyframeEffect interface]: https://cs.chromium.org/search/?q=file:core/animation/keyframe_effect.idl
[KeyframeEffect object]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffect$
[KeyframeEffect constructor]: https://cs.chromium.org/search/?q=function:blink::KeyframeEffect::Create$
[target attribute]: https://cs.chromium.org/search/?q=function:blink::KeyframeEffect::(setT|t)arget$
[pseudoElement attribute]: https://cs.chromium.org/search/?q=function:blink::KeyframeEffect::(setP|p)seudoElement$
[composite attribute]: https://cs.chromium.org/search/?q=function:blink::KeyframeEffect::(setC|c)omposite$
[getKeyframes method]: https://cs.chromium.org/search/?q=function:blink::KeyframeEffect::getKeyframes$
[setKeyframes method]: https://cs.chromium.org/search/?q=function:blink::KeyframeEffect::setKeyframes$
[KeyframeEffectOptions]: https://w3.org/TR/web-animations-1/#the-keyframeeffectoptions-dictionary
[TransitionKeyframes]: https://cs.chromium.org/search/?q=class:blink::TransitionKeyframe$
[StringKeyframes]: https://cs.chromium.org/search/?q=class:blink::StringKeyframe$
[KeyframeEffectModel]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffectModel$
[CssKeyframeEffectModel]: https://cs.chromium.org/search/?q=class:blink::CssKeyframeEffectModel$
[getComputedKeyframes]: https://cs.chromium.org/search/?q=function:blink::(CSS|)KeyframeEffectModel(Base|)::getComputedKeyframes$

### Timeline details

#### AnimationTimeline

The [AnimationTimeline interface][] provides access to the current time and
phase of a timeline. A timeline provides a real (DocumentTimeline) or abstract
(ScrollTimeline) notion of time and is used to synchronize timing updates to
animations.

In addition to supporting the JavaScript interfaces the
[AnimationTimeline class][] has a number of other responsibilities. These
include: 1) maintaining a set of attached animations, 2) keeping
track of which animations require an update during the next cycle, 3)
flagging and removing replaced animations, and 4) retrieving a list of active
animations in support of getAnimations calls.

* **[currentTime attribute](
https://cs.chromium.org/search/?q=function:blink::AnimationTimeline::currentTime$)**:
gets the current time for the timeline or null if unresolved. The current time
may be relative to a time origin in the case of a monotonically increasing
timeline or proportional to the scroll position in the case of a non-monotonic
timeline. The value of currentTime is updated each animation frame. Within the
context of script execution, however, the value returned for currentTime must
remain fixed. This restriction ensures consistent behavior if multiple calls to
fetch the currentTime are performed within a script.

* **[phase attribute](
https://cs.chromium.org/search/?q=function:blink::AnimationTimeline::phase$
)**: gets the phase of a timeline. A timeline that is not associated with the
active document is in the 'inactive' phase. A DocumentTimeline has an additional
constraint that the time origin needs to be initialized for the timeline to be
active. A timeline in the 'inactive' phase has an unresolved value for
currentTime.

* **[duration attribute](https://cs.chromium.org/search/?q=function:blink::AnimationTimeline::duration$)**: gets the duration of
the timeline, which is the maximum value a timeline may generate for its current
time. When using a document timeline the value is unresolved. Scroll timelines
are progress based, and the timeline time has a strict upper bound which is
100%. This value is used to calculate the intrinsic iteration duration for the
target  effect of an animation that is associated with the timeline when the
effect’s iteration duration is auto. The value is computed such that the effect
fills the available time.

[AnimationTimeline interface]: https://w3.org/TR/web-animations-1/#the-animationtimeline-interface
[AnimationTimeline class]: https://cs.chromium.org/search/?q=class:blink::AnimationTimeline$

#### DocumentTimeline

The [DocumentTimeline interface][] extends the [AnimationTimeline interface][]
to add an originTime option for its constructor. The originTime is the time
offset in milliseconds relative to the time origin (zero time) and may be used
to synchronize animations across multiple document timelines.

[DocumentTimeline interface]: https://w3.org/TR/web-animations-1/#the-documenttimeline-interface

#### ScrollTimeline

The [ScrollTimeline interface][] provides additional attributes unique to
scroll-linked animations. These are the scrolling container element, the
scroll direction which drives the timeline, and offsets to determine the active
range.

* **[source attribute](https://cs.chromium.org/search/?q=function:blink::ScrollTimeline::scrollSource$)**: gets the element
whose scroll position drives the progress of the timeline. Per spec, the
attribute is 'source', but it is presently implemented as 'scrollSource'.

* **[orientation attribute](https://cs.chromium.org/search/?q=function:blink::ScrollTimeline::orientation$)**: determines which
scroll axis drives the timeline progress. The value may be one of the following:
horizontal, vertical, inline, block.  The last two options are logical values
and correspond to the language dependent writing direction, and page layout
direction, respectively.

* **[scrollOffsets attribute](https://cs.chromium.org/search/?q=function:blink::ScrollTimeline::scrollOffsets$)**: determines the
range in which the timeline time is active. The value is an array of [container
based offsets][] or [element based offsets][]. By default the range is
[auto, auto] which is equivalent to the container offsets [0%, 100%].

[ScrollTimeline interface]: https://drafts.csswg.org/scroll-animations-1/#scrolltimeline-interface
[container based offsets]: https://drafts.csswg.org/scroll-animations-1/#container-based-offset-section
[element based offsets]: https://drafts.csswg.org/scroll-animations-1/#element-based-offset-section

### Document extension

Each document has an associated DocumentTimeline, which is the default document
timeline that will be used if an element is started via element.animate or
CSS rules. An alternate timeline may be used by calling the Animation
constructor rather than Element.animate to create the animation.

* **[timeline](
https://cs.chromium.org/search/?q=function:blink::Document::Timeline$)**: the
default document timeline.


### DocumentOrShadowRoot extension

Documents and ShadowRoots support an API call to retrieve all animations
associated with the document or shadow root.

* **[getAnimations](
https://cs.chromium.org/search/?q=function:blink::DocumentOrShadowRoot::getAnimations$)**: retrieves the list of all active animations associated with the document
in composite ordering. See Animatable.getAnimations for a brief overview of
composite ordering. DocumentOrShadowRoot::getAnimations calls
[DocumentAnimations::getAnimations][], which walks the timelines associated with
the document and extracts the active ones.

[DocumentAnimations::getAnimations]: https://cs.chromium.org/search/?q=function:blink::DocumentAnimations::getAnimations$


## The interpolation stack

Animation keyframes serve as mileposts indicating property values at specific
points through the progress of the animation. Most commonly, keyframes are
provided for the start and end of the animation; however, an arbitrary number of
keyframes can be added for intermediate points. In fact, the start and ending
points for the animation are not strictly required and if missing neutral
keyframe values (based on the underlying property values) are used. Optionally,
the keyframe can specify the timing function used to indicate the path for
connecting the dots between keyframes. Details for interpolating a scalar valued
property are covered in the section titled 'web animation model'. This
interpolation strategy extends to list valued properties. For example:

   ```css
   @keyframes pulse {
     0%   { filter: brightness(100%),
            animation-timing-function: cubic-bezier(0.2, 0.3, 0.7, 1.0) },
     25%  { filter: brightness(150%)
            animation-timing-function: cubic-bezier(0.2, 0.0, 0.8, 1.0) },
     75%  { filter: brightness(50%)
            animation-timing-function: cubic-bezier(0.3, 0.0, 0.8, 0.7) },
     100% { filter: brightness(100%) },
   }
   button:hover {
     animation: pulse 1s;
   }
   ```

In this example, the brightness filter for the pulse animation follows roughly a
sinusoidal path, approximated by piecewise cubic-bezier curves. Though the
filter property supports a list of filter functions, the values are consistent
between frames with only the brightness changing. Thus, the value can be
interpolated following the same rules as for a scalar, i.e.:

   ```
   value = (1 - p) A + p B
   ```

Each property that can be interpolated has an associated
[CSSInterpolationType][] that performs validation and constructs
[InterpolableValue][]s. For our filter example, the [InterpolableFilter][]s are
created via [CSSFilterListInterpolationType][], which requires a pairwise match
between filter functions to be valid for continuous interpolation.

Various fallback mechanisms are used for interpolation of list-valued properties
that are not pairwise compatible. In some cases, discrete interpolation is used
where the output switch from A to B at a progress value of 0.5.

Transform lists are a particularly interesting example. If the lists are
pairwise compatible between frames, they can be interpolated much like scalars.
For example, transforming between 'scale(1)' and 'scale(2)'. The notion of
pairwise compatibility is extended to classes of functions with similar
geometric properties. For example, 'translateX' and 'translateY' are both
special cases of 'translate' and thus considered pairwise compatible. An
interpolation between 'translateX(100px)' and 'translateY(100px)' is internally
converted to an  interpolation between 'translate(100px, 0)' and 'translate(0,
100px)'. Now that the same transform function is being used, pairwise
interpolation rules apply.

In the general case, incompatible transform list fall back to matrix
interpolation. Any series of transform operations can be expressed as a 4x4
transformation matrix. Unfortunately, information is lost in the process as
a no-op transform is indistinguishable from a 360 degree rotation once expressed
in matrix form. For this reason, we use matrix interpolation as a last resort.

Interpolation of matrices is not as simple as interpolating the matrix elements.
Instead, each matrix is decomposed into a set of transformations that produces
the same matrix representation. Algorithms for decomposing and interpolating
2-D and 3-D matrices are covered in the spec. The
2-D algorithm is covered in [css-transforms-1 - interpolations of matrices][].
The 3-D algorithm is covered in
[css-transforms-2 - interpolations of 3d matrices][]. Note that running the
3D decomposition algorithm on a 2D transformation is not guaranteed to provide a
consistent decomposition as applying the 2D algorithm. The decomposition
process in Blink uses the 2D algorithm if both matrices are 2D and the 3D
algorithm otherwise.

To minimize the use of the matrix fallback, transform lists can be extended
with neutral transform operations for the purpose of pairwise matching and the
matrix fallback only applies to the residuals after pairwise matching. For
example, interpolation between 'translateX(100px) scale(2)' and
'translateY(100px)' is treated as 'translate(100px, 0) scale(2)' to
'translate((0, 100px) scale(1)' establishing pairwise compatibility. An
interpolation between 'scale(1) translateX(100px)' and 'scale(2) rotate(90deg)'
is only partially pairwise compatible. The scale is interpolated pairwise, but
the translate to rotate interpolation is handled via matrix decomposition.
Rules for interpolations of transform lists are covered in
[css-transforms-1 - interpolations of transforms][].

Multiple animations can be applied to a single element. There are strict rules
for how to combine these effects based on composite ordering as well as
composite mode. CSS transitions are applied before CSS animations, which in
turn are applied before animations created via element.animate. Within each
class of animation, there are also ordering rules that depend on the type. For
example,

CSS transitions are ordered by 'transition generation', an index that increases
with each style change. Within a single transition generation, transitions are
sorted by property name.

   ```javascript
   div.style.top = '0px';
   div.style.left = '0px';
   div.style.transition = 'all 100ms';

   div.style.top = '100px';
   div.style.left = '100px';

   // Styles are flushed when retrieving the list of animations. The left and
   // top transitions are created within the same style update and thus sorted
   // alphabetically.
   const transitions = div.getAnimations();
   assert_equals(transitions[0].transitionProperty, 'left');
   assert_equals(transitions[1].transitionProperty, 'top');

   div.style.opacity = 0.5;

   // The opacity transition is at the end since created in a separate style
   // update cycle.
   const updated_transitions = div.getAnimations();
   assert_equals(transitions[0].transitionProperty, 'left');
   assert_equals(transitions[1].transitionProperty, 'top');
   assert_equals(transitions[1].transitionProperty, 'opacity');
   ```

CSS animations that are applied to a single element are ordered by index within
the animation-name property. Pseudo-element selectors have a strict ordering by
selector name. CSS animations applying to different elements are sorted in DOM
order. Note that the DOM order sort is only applied for getAnimations calls as
it is too expensive too apply in general and does not affect rendering if
internally sorted for style calculations in creation order instead.

   ```css
     @keyframes fade { ... }
     @keyframes shrink { ... }
     @keyframes highlight { ... }
     button.dismiss { animation: shrink 600ms linear, fade 600ms ease-out; }
     button:hover::before { animation: highlight 300ms forwards; }
   ```

   ```javascript
     button.onclick = (evt) => {
       const target = evt.target;
       target.classList.add('dismiss');
       const animations = document.getAnimations();
       // The fade and shrink animations are sorted by the ordering in which
       // they appear in the animation property.
       assert_equals(animations[0].animationName, 'shrink');
       assert_equals(animations[1].animationName, 'fade');
       // The pseudo-element animation appears after its parent.
       assert_equals(animations[1].animationName, 'highlight');
     };
   ```

It is also possible for a CSS transition or animation to get treated as a
generic animation if it becomes disassociated from its owning element. The above
example demonstrate that the getAnimations calls retrieve CSS animations and
transitions as well as those generated via Element.animate. A finished CSS
transition or animation is no longer associated with CSS rules that triggered
the animation. If we retain a reference to one of these animations and replay
it, it is treated like a generic animation, which in turn affects sort ordering.


Within the Blink implementation, [EffectStack][] is used to store
sampled effects and sort them for style update. These effects are collected as
follows:

* During each animation frame, [PageAnimator::ServiceScriptedAnimations][] is
  called.
    * The animation clock is updated and will hold its new value until the end
      of the frame.
    * [DocumentAnimations::UpdateAnimationTimingForAnimationFrame] is called
      for each document associated with the main frame.
      * This is turn calls [AnimationTimeline::ServiceAnimations][] for each
        timeline associated with the document.
        * Each timeline calls [Animation::Update][] for each of the
          associated animations
          * The animation update applies the web-animation model and calls
            [KeyframeEffect::ApplyEffects][], which samples the animation and
            adds it to the [EffectStack][]

The method [EffectStack::ActiveInterpolations][] assembles a set of
interpolations for an element applying the effects in the correct order. This
method is called separately for CSS transitions and animations since
transitions are to be applied before animations. Effects are grouped by
property, and since multiple effects can target the same property, the effects
need to be combined in some fashion. The composite mode property determines how
effects are combined. The default mode is 'replace', whereby a new effect for a
property flushes all previous effects for the property.  The other modes are
'add' and 'accumulate'. If either of these options is used, both the old and
new effect are included in the set.

[CSSInterpolationType]: https://cs.chromium.org/search/?q=class:blink::CSSInterpolationType$
[InterpolableValue]: https://cs.chromium.org/search/?q=class:blink::InterpolableValue$
[InterpolableFilter]: https://cs.chromium.org/search/?q=class:blink::InterpolableFilter$
[CSSFilterListInterpolationType]: https://cs.chromium.org/search/?q=class:blink::CSSFilterListInterpolationType$
[EffectStack]: https://cs.chromium.org/search/?q=class:blink::EffectStack$
[PageAnimator::ServiceScriptedAnimations]: https://cs.chromium.org/search/?q=function:blink::PageAnimator::ServiceScriptedAnimations$
[DocumentAnimations::UpdateAnimationTimingForAnimationFrame]: https://cs.chromium.org/search/?q=function:blink::DocumentAnimations::UpdateAnimationTimingForAnimationFrame$
[AnimationTimeline::ServiceAnimations]: https://cs.chromium.org/search/?q=function:blink::AnimationTimeline::ServiceAnimations$
[Animation::Update]: https://cs.chromium.org/search/?q=function:blink::Animation::Update$
[KeyframeEffect::ApplyEffect]: https://cs.chromium.org/search/?q=function:blink::KeyframeEffect::ApplyEffects$
[EffectStack::ActiveInterpolations]: https://cs.chromium.org/search/?q=function:blink::EffectStack::ActiveInterpolations$
[css-transforms-1 - interpolations of matrices]: https://www.w3.org/TR/css-transforms-1/#matrix-interpolation
[css-transforms-2 - interpolations of 3d matrices]: https://www.w3.org/TR/css-transforms-2/#interpolation-of-3d-matrices
[css-transforms-1 - interpolations of transforms]: https://www.w3.org/TR/css-transforms-1/#interpolation-of-transforms


## Integration with Chromium

The Blink animation engine interacts with Blink/Chrome in the following ways:

*   ### [Blink's Style engine](../css)

    The most user visible functionality of the animation engine is animating
    CSS values. This means animations have a place in the [CSS cascade][] and
    influence the [ComputedStyle][]s returned by [ResolveStyle()][].

    The implementation for this lives under [ApplyAnimatedStandardProperties()][]
    for standard properties and [ApplyAnimatedCustomProperties()][] for custom
    properties. Custom properties have more complex application logic due to
    dynamic dependencies introduced by [variable references].

    Animations can be controlled by CSS via the [`animation`](https://www.w3.org/TR/css-animations-1/#animation)
    and [`transition`](https://www.w3.org/TR/css-transitions-1/#transition-shorthand-property) properties.
    In code this happens when [ResolveStyle()][] uses [CalculateAnimationUpdate()][]
    and [CalculateTransitionUpdate()][] to build a [set of mutations][] to make
    to the animation engine which gets [applied later][].

[CSS cascade]: https://www.w3.org/TR/css-cascade-3/#cascade-origin
[ComputedStyle]: https://cs.chromium.org/search/?q=class:blink::ComputedStyle$
[ResolveStyle()]: https://cs.chromium.org/search/?q=function:StyleResolver::ResolveStyle
[ApplyAnimatedStandardProperties()]: https://cs.chromium.org/?type=cs&q=function:StyleResolver::ApplyAnimatedStandardProperties
[ApplyAnimatedCustomProperties()]: https://cs.chromium.org/?type=cs&q=function:ApplyAnimatedCustomProperties
[variable references]: https://www.w3.org/TR/css-variables-1/#using-variables
[CalculateAnimationUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::CalculateAnimationUpdate
[CalculateTransitionUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::CalculateTransitionUpdate
[MaybeApplyPendingUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::MaybeApplyPendingUpdate
[set of mutations]: https://cs.chromium.org/search/?q=class:CSSAnimationUpdate
[applied later]: https://cs.chromium.org/search/?q=function:Element::StyleForLayoutObject+MaybeApplyPendingUpdate

*   ### [Chromium's Compositor](../../../../../cc/README.md)

    Chromium's compositor has a separate, more lightweight [animation
    engine](../../../../../cc/animation/README.md) that runs separate to the
    main thread. Blink's animation engine delegates animations to the compositor
    where possible for better performance and power utilisation.

    #### Compositable animations

    A subset of style properties (currently transform, opacity, filter, and
    backdrop-filter) can be mutated on the compositor thread. Animations that
    mutate only these properties are candidates for being accelerated and run
    on the compositor thread which ensures they are isolated from Blink's main
    thread work.

    Whether or not an animation can be accelerated is determined by
    [CheckCanStartAnimationOnCompositor()][] which looks at several aspects
    such as the composite mode, other animations affecting same property, and
    whether the target element can be promoted and mutated in compositor.
    Reasons for not compositing animations are captured in [FailureCodes][].

    #### Lifetime of a compositor animation

    Animations that can be accelerated get added to the [PendingAnimations][]
    list. The pending list is updated as part of document lifecycle and ensures
    each pending animation gets a corresponding [cc::Animation][]
    representing the animation on the compositor. The player is initialized with
    appropriate timing values and corresponding effects.

    Note that changing that animation playback rate, start time, or effect,
    simply adds the animation back on to the pending list and causes the
    compositor animation to be cancelled and a new one to be started. See
    [Animation::PreCommit()][] for more details.

    An accelerated animation is still running on main thread ensuring that its
    effective output is reflected in the Element style. So while the compositor
    animation updates the visuals the main thread animation updates the computed
    style. There is a special case logic to ensure updates from such accelerated
    animations do not cause spurious commits from main to compositor (See
    [CompositedLayerMapping::UpdateGraphicsLayerGeometry()][], or
    [FragmentPaintPropertyTreeBuilder::UpdateTransform()][],
    [FragmentPaintPropertyTreeBuilder::UpdateEffect()][], and
    [FragmentPaintPropertyTreeBuilder::UpdateFilter()][] for
    [BlinkGenPropertyTrees mode][])

    A compositor animation provides updates on its playback state changes (e.g.,
    on start, finish, abort) to its blink counterpart via
    [CompositorAnimationDelegate][] interface. Blink animation uses the start
    event callback to obtain an accurate start time for the animation which is
    important to ensure its output accurately reflects the compositor animation
    output.

[CheckCanStartAnimationOnCompositor()]: https://cs.chromium.org/search/?q=file:animation.h+function:CheckCanStartAnimationOnCompositor
[FailureCodes]: https://cs.chromium.org/search/?q=return%5Cs%2B(CompositorAnimations::)?FailureCode
[cc::AnimationPlayer]: https://cs.chromium.org/search/?q=file:src/cc/animation/animation_player.h+class:AnimationPlayer
[PendingAnimations]: https://cs.chromium.org/search/?q=file:pending_animations.h+class:PendingAnimations
[Animation::PreCommit()]: https://cs.chromium.org/search/?q=file:animation.h+function:PreCommit
[CompositorAnimationDelegate]: https://cs.chromium.org/search/?q=file:compositor_animation_delegate.h
[CompositedLayerMapping::UpdateGraphicsLayerGeometry()]: https://cs.chromium.org/search/?q=file:composited_layer_mapping.h+function:UpdateGraphicsLayerGeometry
[FragmentPaintPropertyTreeBuilder::UpdateTransform()]: https://cs.chromium.org/search/?q=class:FragmentPaintPropertyTreeBuilder+function:UpdateTransform
[FragmentPaintPropertyTreeBuilder::UpdateEffect()]: https://cs.chromium.org/search/?q=class:FragmentPaintPropertyTreeBuilder+function:UpdateEffect
[FragmentPaintPropertyTreeBuilder::UpdateFilter()]: https://cs.chromium.org/search/?q=class:FragmentPaintPropertyTreeBuilder+function:UpdateFilter
[BlinkGenPropertyTrees mode]: https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/paint/README.md

*   ### Javascript

    [EffectInput](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/animation/effect_input.cc)
    contains the helper functions that are used to
    [process a keyframe argument](https://w3.org/TR/web-animations-1/#processing-a-keyframes-argument)
    which can take an argument of either object or array form.

*   ### DevTools

    The animations timeline uses [InspectorAnimationAgent][] to track all active
    animations. This class has interfaces for pausing, adjusting
    DocumentTimeline playback rate, and seeking animations.

    InspectorAnimationAgent clones the inspected animation in order to avoid
    firing animation events, and suppresses the effects of the original
    animation. From this point on, modifications can be made to the cloned
    animation without having any effect on the underlying animation or its
    listeners.

[InspectorAnimationAgent]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/inspector/InspectorAnimationAgent.h

*   ### SVG

    The `element.animate()` API supports targeting SVG attributes in its
    keyframes. This is an experimental implementation guarded by the
    WebAnimationsSVG flag and not exposed on the web.

    This feature should provide a high fidelity alternative to our SMIL
    implementation.

## Architecture

### Animation Timing Model

The animation engine is built around the
[timing model](https://www.w3.org/TR/web-animations-1/#timing-model) described
in the Web Animations spec.

This describes a hierarchy of entities:

*   __[DocumentTimeline][]__: Represents the wall clock time.
    *   __[Animation][]__: Represents an individual animation and when it
        started playing.
        *   __[AnimationEffect][]__: Represents the effect an animation has
            during the animation (e.g. updating an element's color property).

Time trickles down from the [DocumentTimeline][] and is transformed at each
stage to produce some progress fraction that can be used to apply the effects of
the animations.

For example:

```javascript
// Page was loaded at 2:00:00PM, the time is currently 2:00:10PM.
// document.timeline.currentTime is currently 10000 (10 seconds).

let animation = element.animate([
    {transform: 'none'},
    {transform: 'rotate(200deg)'},
  ], {
    duration: 20000,  // 20 seconds
  });

animation.startTime = 6000;  // 6 seconds
```

*   __[DocumentTimeline][]__ notifies that the time is 10 seconds.
    *   __[Animation][]__ computes that its currentTime is 4 seconds due to its
        startTime being at 6 seconds.
        *   __[AnimationEffect][]__ has a duration of 20 seconds and computes
            that it has a progress of 20% from the parent animation being 4
            seconds into the animation.

            The effect is animating an element from `transform: none` to
            `transform: rotate(200deg)` so it computes the current effect to be
            `transfrom: rotate(40deg)`.

[Animation]: https://cs.chromium.org/search/?q=class:blink::Animation$
[AnimationEffect]: https://cs.chromium.org/search/?q=class:blink::AnimationEffect$
[DocumentTimeline]: https://cs.chromium.org/search/?q=class:blink::DocumentTimeline$
[EffectStack]: https://cs.chromium.org/search/?q=class:blink::EffectStack

### Lifecycle of an Animation

![Lifecycle]

1.  An [Animation][] is created via CSS<sup>1</sup> or `element.animate()`.
2.  At the start of the next frame the [Animation][] and its [AnimationEffect][]
    are updated with the currentTime of the [DocumentTimeline][].
3.  The [AnimationEffect][] gets sampled with its computed localTime, pushes a
    [SampledEffect][] into its target element's [EffectStack][] and marks the
    elements style as dirty to ensure it gets updated later in the document
    lifecycle.
4.  During the next [style resolve][ResolveStyle()] on the target element all
    the [SampledEffect][]s in its [EffectStack][] are incorporated into building
    the element's [ComputedStyle][].

One key takeaway here is to note that timing updates are done in a separate
phase to effect application. Effect application must occur during style
resolution which is a highly complex process with a well defined place in the
document lifecycle. Updates to animation timing will request style updates
rather than invoke them directly.

<sup>1</sup> CSS animations and transitions are actually created/destroyed
during style resolve (step 4). There is special logic for forcing these
animations to have their timing updated and their effects included in
the same style resolve. An unfortunate side effect of this is that style
resolution can cause style to get dirtied, this is currently a
[code health bug](http://crbug.com/492887).

[Lifecycle]: images/lifecycle.png
[SampledEffect]: https://cs.chromium.org/search/?q=class:blink::SampledEffect

### [KeyframeEffect][]

Currently all animations use [KeyframeEffect][] for their [AnimationEffect][].
The generic [AnimationEffect][] from which it inherits is an extention point in
Web Animations where other kinds of animation effects can be defined later by
other specs (for example Javascript callback based effects).

#### Structure of a [KeyframeEffect][]

*   __[KeyframeEffect][]__ represents the effect an animation has (without any
    details of when it started or whether it's playing) and is comprised of
    three things:
    *   Some __[Timing][]__ information (inherited from [AnimationEffect][]).
        [Example](http://jsbin.com/nuyohulojo/edit?js,output):
        ```javascript
        {
          duration: 4000,
          easing: 'ease-in-out',
          iterations: 8,
          direction: 'alternate',
        }
        ```
        This is used to [compute][UpdateInheritedTime()] the percentage progress
        of the effect given the duration of time that the animation has been
        playing for.

    *   The DOM __[Element][]__ that is being animated.

    *   A __[KeyframeEffectModel][]__ that holds a sequence of keyframes to
        specify the properties being animated and what values they pass
        through.
        [Example](http://jsbin.com/wiyefaxiru/1/edit?js,output):
        ```javascript
        [
          {backgroundColor: 'red', transform: 'rotate(0deg)'},
          {backgroundColor: 'yellow'},
          {backgroundColor: 'lime'},
          {backgroundColor: 'blue'},
          {backgroundColor: 'red', transform: 'rotate(360deg)'},
        ]
        ```

        These keyframes are used to compute:
        *   A __[PropertySpecificKeyframe map][KeyframeGroupMap]__ that simply
            breaks up the input multi-property keyframes into per-property
            keyframe lists.
        *   An __[InterpolationEffect][]__ which holds a set of
            [Interpolation][]s, each one representing the animated values
            between adjacent pairs of [PropertySpecificKeyframe][]s, and where
            in the percentage progress they are active.
            In the example keyframes above the [Interpolations][] generated
            would include, among the 5 different property specific keyframe
            pairs, one for `backgroundColor: 'red'` to
            `backgroundColor: 'yellow'` that applied from 0% to 25% and one for
            `transform: 'rotate(0deg)'` to `transform: 'rotate(360deg)'` that
            applied from 0% to 100%.

[Element]: https://cs.chromium.org/search/?q=class:blink::Element$
[KeyframeGroupMap]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffectModelBase+KeyframeGroupMap
[PropertySpecificKeyframe]: https://cs.chromium.org/search/?q=class:blink::Keyframe::PropertySpecificKeyframe
[KeyframeEffect]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffect$
[KeyframeEffectModel]: https://cs.chromium.org/search/?q=class:blink::KeyframeEffectModelBase$
[Timing]: https://cs.chromium.org/search/?q=class:blink::Timing$
[UpdateInheritedTime()]: https://cs.chromium.org/search/?q=function:%5CbAnimationEffect::UpdateInheritedTime

#### Lifecycle of an [Interpolation][]

[Interpolation][] is the data structure that [style
resolution][ResolveStyle()] uses to resolve what animated value to apply
to an animated element's [ComputedStyle][].

1.   [Interpolation][]s are lazily
     [instantiated][EnsureInterpolationEffectPopulated()] prior to sampling.
2.   [KeyframeEffectModel][]s are [sampled][Sample()] every frame (or as
     necessary) for a stack of [Interpolation][]s to
     [apply][ApplyAnimatedStandardProperties()] to the associated [Element][]
     and stashed away in the [Element][]'s [ElementAnimations][]'
     [EffectStack][]'s [SampledEffect][]s.
3.   During [style resolution][ResolveStyle()] on the target [Element][] all
     the [Interpolation][]s are [collected and organised by
     category][AdoptActiveInterpolations] according to whether it's a transition
     or not (transitions in Blink are
     [suppressed][CalculateTransitionUpdateForProperty()] in the presence of
     non-transition animations on the same property) and whether it affects
     custom properties or not (animated custom properties are
     [animation-tainted](https://www.w3.org/TR/css-variables-1/#animation-tainted)
     and affect the [processing of animation
     properties][animation-tainted-processing].
4.   TODO(alancutter): Describe what happens in processing a stack of
     interpolations.

[AdoptActiveInterpolations]: https://cs.chromium.org/search/?q=AdoptActiveInterpolations%5Cw%2B
[animation-tainted-processing]: https://cs.chromium.org/search/?q=function:blink::StyleBuilder::ApplyProperty+animation_tainted
[CalculateTransitionUpdateForProperty()]: https://cs.chromium.org/search/?q=function:blink::CSSAnimations::CalculateTransitionUpdateForProperty
[ElementAnimations]: https://cs.chromium.org/search/?q=class:blink::ElementAnimations
[EnsureInterpolationEffectPopulated()]: https://cs.chromium.org/search/?q=function:KeyframeEffectModelBase::EnsureInterpolationEffectPopulated
[Interpolation]: https://cs.chromium.org/search/?q=class:blink::Interpolation$
[InterpolationEffect]: https://cs.chromium.org/search/?q=class:blink::InterpolationEffect
[Sample()]: https://cs.chromium.org/search/?q=function:KeyframeEffectModelBase::Sample

## Testing pointers

Test new animation features using end to end web-platform-tests to ensure
cross-browser interoperability. Use unit testing when access to chrome internals
is required. Test chrome specific features such as compositing of animation
using web tests or unit tests.

### End to end testing

Features in the Web Animations spec are tested in
[web-animations][web-animations-tests]. [Writing web platform tests][] has
pointers for how to get started. If Chrome does not correctly implement the
spec, add a corresponding -expected.txt file with your test listing the expected
failure in Chrome.

[Web tests](../../../../../docs/testing/writing_web_tests.md) are located
in [third_party/blink/web_tests][]. These should be written when needing end
to end testing but either when testing chrome specific features (i.e.
non-standardized) such as compositing or when the test requires access to chrome
internal features not easily tested by web-platform-tests.

[web-animations-tests]: https://cs.chromium.org/chromium/src/third_party/blink/web_tests/external/wpt/web-animations/
[Writing web platform tests]: ../../../../../docs/testing/web_platform_tests.md#Writing-tests
[third_party/blink/web_tests]: https://cs.chromium.org/chromium/src/third_party/blink/web_tests/animations/

### Unit testing

Unit testing of animations can range from [extending Test][] when you will
manually construct an instance of your object to [extending RenderingTest][]
where you can load HTML, [enable compositing][] if necessary, and run assertions
about the state.

[extending Test]: https://cs.chromium.org/search/?q=public%5C+testing::Test+file:core%5C/animation&sq=package:chromium&type=cs
[extending RenderingTest]: https://cs.chromium.org/search/?q=public%5C+RenderingTest+file:core%5C/animation&type=cs
[enable compositing]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/animation/compositor_animations_test.cc?type=cs&sq=package:chromium&q=file:core%5C/animation%5C/.*test%5C.cpp+EnableCompositing

## Ongoing work

### Properties And Values API

TODO: Summarize properties and values API.

### [Animation Worklet](../../modules/animationworklet/README.md)

AnimationWorklet is a new primitive for creating high performance procedural
animations on the web. It is being incubated as part of the
[CSS Houdini task force](https://github.com/w3c/css-houdini-drafts/wiki), and if
successful will be transferred to that task force for full standardization.

A [WorkletAnimation][] behaves and exposes the same animation interface as other
web animation but it allows the animation itself to be highly customized in
Javascript by providing an `animate` callback. These animations run inside an
isolated worklet global scope.

[WorkletAnimation]: https://cs.chromium.org/search/?q=file:animationworklet/worklet_animation.h+class:WorkletAnimation
