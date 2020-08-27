/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_USE_COUNTER_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_USE_COUNTER_HELPER_H_

#include <bitset>
#include "base/macros.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class DocumentLoader;
class Element;
class LocalFrame;

// Utility class for muting UseCounter, for instance ignoring attributes
// constructed in user-agent shadow DOM. Once constructed, all UseCounting
// is muted, until the object is destroyed again. It is the callees
// responsibility to make sure this happens.
class UseCounterMuteScope {
  STACK_ALLOCATED();

 public:
  UseCounterMuteScope(const Element& element);
  ~UseCounterMuteScope();

 private:
  DocumentLoader* loader_;
};

// This class provides an implementation of UseCounter - see the class comment
// of blink::UseCounter for the feature.
// Changes on UseCounterHelper are observable by UseCounterHelper::Observer.
class CORE_EXPORT UseCounterHelper final {
  DISALLOW_NEW();

 public:
  // The context determines whether a feature is reported to UMA histograms. For
  // example, when the context is set to kDisabledContext, no features will be
  // reported to UMA, but features may still be marked as seen to avoid multiple
  // console warnings for deprecation.
  enum Context {
    kDefaultContext,
    // Counters for extensions.
    kExtensionContext,
    // Context for file:// URLs.
    kFileContext,
    // Context when counters should be disabled (eg, internal pages such as
    // about, devtools, etc).
    kDisabledContext
  };

  enum CommitState { kPreCommit, kCommited };

  // CSS properties for animation are separately counted. This enum is used to
  // distinguish them.
  enum class CSSPropertyType { kDefault, kAnimation };

  explicit UseCounterHelper(Context = kDefaultContext,
                            CommitState = kPreCommit);

  // An interface to observe UseCounterHelper changes. Note that this is never
  // notified when the counter is disabled by |m_muteCount| or when |m_context|
  // is kDisabledContext.
  class Observer : public GarbageCollected<Observer> {
   public:
    // Notified when a feature is counted for the first time. This should return
    // true if it no longer needs to observe changes so that the counter can
    // remove a reference to the observer and stop notifications.
    virtual bool OnCountFeature(WebFeature) = 0;

    virtual void Trace(Visitor* visitor) const {}
  };

  // Repeated calls are ignored.
  void Count(CSSPropertyID, CSSPropertyType, const LocalFrame*);
  // Repeated calls are ignored.
  void Count(WebFeature, const LocalFrame*);

  bool IsCounted(CSSPropertyID unresolved_property, CSSPropertyType) const;

  // Retains a reference to the observer to notify of UseCounterHelper changes.
  void AddObserver(Observer*);

  // Invoked when a new document is loaded into the main frame of the page.
  void DidCommitLoad(const LocalFrame*);

  // When muted, all calls to "count" functions are ignoed.  May be nested.
  void MuteForInspector();
  void UnmuteForInspector();

  void RecordMeasurement(WebFeature, const LocalFrame&);
  void ReportAndTraceMeasurementByFeatureId(WebFeature, const LocalFrame&);
  void ReportAndTraceMeasurementByCSSSampleId(int,
                                              const LocalFrame*,
                                              bool /*is_animated*/);

  // Return whether the feature has been seen since the last page load
  // (except when muted).  Does include features seen in documents which have
  // reporting disabled.
  bool HasRecordedMeasurement(WebFeature) const;

  void ClearMeasurementForTesting(WebFeature);

  void Trace(Visitor*) const;

 private:
  friend class UseCounterHelperTest;

  // Notifies that a feature is newly counted to |m_observers|. This shouldn't
  // be called when the counter is disabled by |m_muteCount| or when |m_context|
  // if kDisabledContext.
  void NotifyFeatureCounted(WebFeature);

  void CountFeature(WebFeature) const;

  // If non-zero, ignore all 'count' calls completely.
  unsigned mute_count_;

  // The scope represented by this UseCounterHelper instance, which must be
  // fixed for the duration of a page but can change when a new page is loaded.
  Context context_;
  // CommitState tracks whether navigation has commited. Prior to commit,
  // UseCounters are logged locally and delivered to the browser only once the
  // document has been commited (eg. to ensure never logging a feature that has
  // no corresponding PageVisits).
  CommitState commit_state_;

  // Track what features/properties have been recorded.
  std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>
      features_recorded_;

  static constexpr size_t kMaxSample =
      static_cast<size_t>(mojom::CSSSampleId::kMaxValue) + 1;
  std::bitset<kMaxSample> css_recorded_;
  std::bitset<kMaxSample> animated_css_recorded_;

  HeapHashSet<Member<Observer>> observers_;

  DISALLOW_COPY_AND_ASSIGN(UseCounterHelper);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_USE_COUNTER_HELPER_H_
