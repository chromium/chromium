// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_H_

#include "base/callback.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
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

  static IntersectionObserver* Create(const IntersectionObserverInit&,
                                      IntersectionObserverDelegate&,
                                      ExceptionState&);
  static IntersectionObserver* Create(ScriptState*,
                                      V8IntersectionObserverCallback*,
                                      const IntersectionObserverInit&,
                                      ExceptionState&);
  static IntersectionObserver* Create(const Vector<Length>& root_margin,
                                      const Vector<float>& thresholds,
                                      Document*,
                                      EventCallback,
                                      DOMHighResTimeStamp delay = 0,
                                      bool track_visbility = false,
                                      ExceptionState& = ASSERT_NO_EXCEPTION);
  static void ResumeSuspendedObservers();

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

  // An observer can either track intersections with an explicit root Element,
  // or with the the top-level frame's viewport (the "implicit root").  When
  // tracking the implicit root, root_ will be null, but because root_ is a
  // weak pointer, we cannot surmise that this observer tracks the implicit
  // root just because root_ is null.  Hence root_is_implicit_.
  bool RootIsImplicit() const { return root_is_implicit_; }

  DOMHighResTimeStamp GetTimeStamp() const;
  DOMHighResTimeStamp GetEffectiveDelay() const;
  const Length& TopMargin() const { return top_margin_; }
  const Length& RightMargin() const { return right_margin_; }
  const Length& BottomMargin() const { return bottom_margin_; }
  const Length& LeftMargin() const { return left_margin_; }
  unsigned FirstThresholdGreaterThan(float ratio) const;
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
  explicit IntersectionObserver(IntersectionObserverDelegate&,
                                Element*,
                                const Vector<Length>& root_margin,
                                const Vector<float>& thresholds,
                                DOMHighResTimeStamp delay,
                                bool track_visibility);
  void ClearWeakMembers(Visitor*);

  const TraceWrapperMember<IntersectionObserverDelegate> delegate_;
  WeakMember<Element> root_;
  HeapLinkedHashSet<WeakMember<IntersectionObservation>> observations_;
  Vector<float> thresholds_;
  DOMHighResTimeStamp delay_;
  Length top_margin_;
  Length right_margin_;
  Length bottom_margin_;
  Length left_margin_;
  unsigned root_is_implicit_ : 1;
  unsigned track_visibility_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_H_
