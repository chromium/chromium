// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_H_

#include "base/callback.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class IntersectionObserverDelegate;
class IntersectionObserverInit;
class ScriptState;
class V8IntersectionObserverCallback;

class CORE_EXPORT IntersectionObserver final
    : public ScriptWrappable,
      public ActiveScriptWrappable<IntersectionObserver>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(IntersectionObserver);
  DEFINE_WRAPPERTYPEINFO();

 public:
  using EventCallback = base::RepeatingCallback<void(
      const HeapVector<Member<IntersectionObserverEntry>>&)>;

  // The IntersectionObserver can be configured to notify based on changes to
  // how much of the target element's area intersects with the root, or based on
  // changes to how much of the root element's area intersects with the
  // target. Examples illustrating the distinction:
  //
  //     1.0 of target,         0.5 of target,         1.0 of target,
  //      0.25 of root           0.5 of root            1.0 of root
  //  +------------------+   +------------------+   *~~~~~~~~~~~~~~~~~~*
  //  |   //////////     |   |                  |   ;//////////////////;
  //  |   //////////     |   |                  |   ;//////////////////;
  //  |   //////////     |   ;//////////////////;   ;//////////////////;
  //  |                  |   ;//////////////////;   ;//////////////////;
  //  +------------------+   *~~~~~~~~~~~~~~~~~~*   *~~~~~~~~~~~~~~~~~~*
  //                         ////////////////////
  //                         ////////////////////
  //                         ////////////////////
  enum ThresholdInterpretation { kFractionOfTarget, kFractionOfRoot };

  // This value can be used to detect transitions between non-intersecting or
  // edge-adjacent (i.e., zero area) state, and intersecting by any non-zero
  // number of pixels.
  static const float kMinimumThreshold;

  // Used to specify when callbacks should be invoked with new notifications.
  // Blink-internal users of IntersectionObserver will have their callbacks
  // invoked synchronously at the end of a lifecycle update. Javascript
  // observers will PostTask to invoke their callbacks.
  enum DeliveryBehavior {
    kDeliverDuringPostLifecycleSteps,
    kPostTaskToDeliver
  };

  static IntersectionObserver* Create(const IntersectionObserverInit*,
                                      IntersectionObserverDelegate&,
                                      ExceptionState&);
  static IntersectionObserver* Create(ScriptState*,
                                      V8IntersectionObserverCallback*,
                                      const IntersectionObserverInit*,
                                      ExceptionState&);

  // Creates an IntersectionObserver that monitors changes to the intersection
  // between its target element relative to its implicit root and notifies via
  // the given |callback|. |thresholds| should be in the range [0,1], and are
  // interpreted according to the given |semantics|. |delay| specifies the
  // minimum period between change notifications.
  //
  // TODO(crbug.com/915495): The |delay| feature is broken. See comments in
  // intersection_observation.cc.
  static IntersectionObserver* Create(
      const Vector<Length>& root_margin,
      const Vector<float>& thresholds,
      Document* document,
      EventCallback callback,
      DeliveryBehavior behavior = kDeliverDuringPostLifecycleSteps,
      ThresholdInterpretation semantics = kFractionOfTarget,
      DOMHighResTimeStamp delay = 0,
      bool track_visbility = false,
      bool always_report_root_bounds = false,
      ExceptionState& = ASSERT_NO_EXCEPTION);

  static void ResumeSuspendedObservers();

  explicit IntersectionObserver(IntersectionObserverDelegate&,
                                Element*,
                                const Vector<Length>& root_margin,
                                const Vector<float>& thresholds,
                                ThresholdInterpretation semantics,
                                DOMHighResTimeStamp delay,
                                bool track_visibility,
                                bool always_report_root_bounds);

  // API methods.
  void observe(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  void unobserve(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  void disconnect(ExceptionState& = ASSERT_NO_EXCEPTION);
  HeapVector<Member<IntersectionObserverEntry>> takeRecords(ExceptionState&);

  // API attributes.
  Element* root() const { return root_.Get(); }
  String rootMargin() const;
  const Vector<float>& thresholds() const { return thresholds_; }
  DOMHighResTimeStamp delay() const { return delay_; }
  bool trackVisibility() const { return track_visibility_; }
  bool trackFractionOfRoot() const { return track_fraction_of_root_; }

  // An observer can either track intersections with an explicit root Element,
  // or with the the top-level frame's viewport (the "implicit root").  When
  // tracking the implicit root, root_ will be null, but because root_ is a
  // weak pointer, we cannot surmise that this observer tracks the implicit
  // root just because root_ is null.  Hence root_is_implicit_.
  bool RootIsImplicit() const { return root_is_implicit_; }

  bool AlwaysReportRootBounds() const { return always_report_root_bounds_; }
  bool NeedsOcclusionTracking() const {
    return trackVisibility() && !observations_.IsEmpty();
  }

  DOMHighResTimeStamp GetTimeStamp() const;
  DOMHighResTimeStamp GetEffectiveDelay() const;
  const Vector<Length>& RootMargin() const { return root_margin_; }
  const Length& TopMargin() const { return root_margin_[0]; }
  const Length& RightMargin() const { return root_margin_[1]; }
  const Length& BottomMargin() const { return root_margin_[2]; }
  const Length& LeftMargin() const { return root_margin_[3]; }

  bool ComputeIntersections(unsigned flags);

  void SetNeedsDelivery();
  DeliveryBehavior GetDeliveryBehavior() const;
  void Deliver();

  // Returns false if this observer has an explicit root element which has been
  // deleted; true otherwise.
  bool RootIsValid() const;

  // ScriptWrappable override:
  bool HasPendingActivity() const override;

  void Trace(blink::Visitor*) override;

  // Enable/disable throttling of visibility checking, so we don't have to add
  // sleep() calls to tests to wait for notifications to show up.
  static void SetThrottleDelayEnabledForTesting(bool);

 private:
  void ProcessCustomWeakness(const WeakCallbackInfo&);

  const Member<IntersectionObserverDelegate> delegate_;
  UntracedMember<Element> root_;
  HeapLinkedHashSet<WeakMember<IntersectionObservation>> observations_;
  Vector<float> thresholds_;
  DOMHighResTimeStamp delay_;
  Vector<Length> root_margin_;
  unsigned root_is_implicit_ : 1;
  unsigned track_visibility_ : 1;
  unsigned track_fraction_of_root_ : 1;
  unsigned always_report_root_bounds_ : 1;
  unsigned needs_delivery_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_H_
