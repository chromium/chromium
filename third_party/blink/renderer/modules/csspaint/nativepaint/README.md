This module handles native paint worklet animations, a feature by which
animations or transitions of css properties are moved from the main thread to
the cc thread by mains of native-code [paint worklets](https://developer.mozilla.org/en-US/docs/Web/API/CSS_Painting_API).
This can be used to composite animations which otherwise would be difficult due
to the lack of applicable layer properties to mutate and a lot of boilerplate
code.

This file serves as both a high level design and a shopping list of classes to
add/modify to composite a new animation type.

# Core concepts

**PaintWorkletDeferredImage**

The Paint Worklet Deferred image is a way of signalling at paint time that
some painting is to be fulfilled on the compositor thread. The 'image', contains
a paint worklet id, as well as a PaintWorkletInput. The id, as well as the
PaintWorkletInputType, is used to determine at runtime:

  1. Which ticking animation is associated with each instance of paint worklet
  2. Which native implementation is associated with each animation

(See [PaintWorkletProxyClient::Paint](https://source.chromium.org/search?q=PaintWorkletProxyClient::Paint%20filepath:paint_worklet_proxy_client.cc&ss=chromium%2Fchromium%2Fsrc))

**CompositedPaintStatus**

CompositedPaintStatus is a tracking variable in [blink::ElementAnimations](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/animation/element_animations.h?q=ElementAnimations%20filepath:element_animations.h%20filepath:third_party&ss=chromium%2Fchromium%2Fsrc)
that tracks whether a particular element has a composited animation or not (see
comment in element_animations.h for more detail on each of the 4 states). This
value is typically re-set to kNoAnimation or kNeedsRepaint during an animation
update and is set to kComposited or kNotComposited during pre-paint or paint
(depending on animation type). The kNeedsRepaint state is necessary because
not all the information to composite an animation is available during an
animation update, and only during paint.

# Core classes

The painting behaviors for composited clip path animation are mediated by the
*PaintDefinition classes. The class hierarchy is as follows

```
                +---------------------------+
                | NativePaintImageGenerator |
                +---------------------------+
                              ^
                              |
                              | (inherits)
                   +----------------------+
                   | *PaintImageGenerator |<------------------------\
                   +----------------------+                          |
                              ^                                      |
                              |             (initializes constructor)|
(renderer/core)               |                          +---------------------+
---------------------------------------------------------| modules_initializer |
(csspaint/nativepaint)        |  _______________________/+---------------------+
                _____________/  /                         (obtains constructor)
                | (inherits)   L
  +--------------------------+                    +------------------+
  | *PaintImageGeneratorImpl |------------------->| *PaintDefinition |
  +--------------------------+ (contains)         +------------------+
                              ____________________/ (inherits) | (contains as
                              |                                |  private inner
                              V                                V  class)
                 +--------------------------+    +--------------------+
                 | NativeCssPaintDefinition |    | *PaintWorkletInput |
                 +--------------------------+    +--------------------+
```

**\*PaintWorkletInput**

The PaintWorkletInput, for a composited animation, contains all the information
necessary to paint animation frames for the entire active and delayed portion
of the animation. This includes keyframes, timing functions, and potentially
the original property value depending on the animation fill mode.

Base class: [PaintWorkletInput](https://source.chromium.org/search?q=PaintWorkletInput%20filepath:paint_worklet_input%20filepath:third_party&sq=&ss=chromium%2Fchromium%2Fsrc)

**\*PaintImageGenerator and \*PaintImageGeneratorImpl**

The Paint Image Generator is responsible for returning a
[PaintWorkletDeferredImage](https://source.chromium.org/search?q=PaintWorkletDeferredImage%20filepath:paint_worklet_deferred_image&ss=chromium%2Fchromium%2Fsrc)
with a valid PaintWorkletInput. Presently, these methods are statically
implemented in the property-specific *PaintDefinition classes. As such, the
Generator/GeneratorImpl distinction exists so that these can be called/created
across the module boundary.

The PaintImageGenerator also contains a method to get the animation if it is
compositible, filtering out most non-compositable animations. This is also
presently implemented in the Paint Definition.

Base class [NativePaintImageGenerator](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/css/native_paint_image_generator.h?q=NativePaintImageGenerator%20filepath:native_paint_image_generator&ss=chromium%2Fchromium%2Fsrc)

**\*PaintDefinition**

The Paint Definition has three key responsibilities:

1. Producing the PaintWorkletInput at paint time
2. Filtering the animation at paint time by implementing
GetAnimationIfCompositable
  * This uses helpers defined in NativeCssPaintDefinition, which implements
  filtering common to all composited animations.
  * Custom logic for filtering non-compositable animations is typically
  implemented as a value filter, see comments in native_css_paint_definition.h
3. Given a PaintWorkletInput, and the global animation progress, produce a
PaintRecord with the correct animation frame.

# Life of a Composited Animation

**On creation**

(On Animation Update)

When a compositable animation is set pending, the composited paint status of
the associate element is set to kNeedsRepaint allowing paint to make the final
compositing decision.

See: ElementAnimations::RecalcCompositedStatus.

(On Style Recalc)

Changes in the composited property are reflected in flags in StyleDifference
(see StyleDifference::PropertyDifference ). These do not by themselves
invalidate layout/paint, this is done via AdjustForCompositableAnimationPaint
(layout_object.h). When an animation is first added, the state should
always be kNeedsRepaint or kNoAnimation, which will result in an unconditional
repaint.

(On Paint/Pre-paint)

The CompositedPaintStatus, if it is kNeedsRepaint, is definitely resolved. In
this case, a call to GetAnimationIfCompositable (checks conditions specific to
native paint worklets) is run, as well as a call to
CheckCanStartAnimationOnCompositor (checks common to all compositor animations).
If both methods show no errors, the state is set to kComposited.

If the animation type requires a paint property to be set, this will be done at
pre-paint. Otherwise, this will be done during paint.

(On Paint)

If the animation is composited, a PaintWorkletDeferredImage is created, with
a PaintWorkletInput containing all the keyframes, timing functions, and other
information necessary to composite the animation.

(On Pre-Commit)

When sending the animation to the compositor thread, a float-based curve is used
instead of a property-specific curve, as the property specific interpolation is
handled in the paint worklet rather than defining a custom curve. Additionally,
the keyframes represent a simple 0->1 linear transformation that represents the
animation's entire active phase. The actual keyframes are not snapshotted or
used, as that would result in errors when attempting to resolve timing
functions.

See CompositorAnimations::GetAnimationOnCompositor

**On Compositor Frame**

(On Animation Update)

Paint worklets are invalidated via AnimatedPaintWorkletTracker (note: to avoid
unnecessary invalidations, a special method called ValueChangeShouldCauseRepaint
is called which ensures timing functions are taken into account).

See: [PictureLayerImpl::InvalidatePaintWorklets](https://source.chromium.org/search?q=PictureLayerImpl::InvalidatePaintWorklets)
See: [AnimatedPaintWorkletTracker::OnCustomPropertyMutated](https://source.chromium.org/search?q=AnimatedPaintWorkletTracker::OnCustomPropertyMutated%20filepath:cc&sq=)

(On Impl-Side Invalidation)

All dirty paint worklets (those without current PaintRecord) are gathered in
[LayerTreeHostImpl::UpdateSyncTreeAfterCommitOrImplSideInvalidation](https://source.chromium.org/search?q=UpdateSyncTreeAfterCommitOrImplSideInvalidation%20%20filepath:.cc&sq=)
Native paint worklets (unlike CSS paint worklets) are painted directly on
compositor, calling the requisite method in PaintDefinition, which is
responsible for computing the intra-frame progress and doing the actual paint.

We can paint native paint worklets directly on compositor as it is known to be
fast, and paint objects do not require GC.

**On Main Thread Frame**

(On Style Recalc)

As before, changes in the composited property are reflected in StyleDifference,
as before. However, because the status is set to kComposited for compositable
animations, no invalidation occurs.

**On Keyframe Change**

See: [CSSAnimations::CalculateCompositorAnimationUpdate](https://source.chromium.org/search?q=CSSAnimations::CalculateCompositorAnimationUpdate%20filepath:css_animations)

**On Animation end/cancel**

(On Animation Update)

CompositedPaintStatus will be set to kNoAnimation (if it is the only composited
animation on the element, kNeedsRepaint otherwise) and invalidate paint,
returning standard main thread behavior.
