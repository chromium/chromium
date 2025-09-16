// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver_with_tracker.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

class MODULES_EXPORT BrowserCaptureMediaStreamTrack
    : public MediaStreamTrackImpl {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BrowserCaptureMediaStreamTrack(ExecutionContext* execution_context,
                                 MediaStreamComponent* component,
                                 base::OnceClosure callback);

  BrowserCaptureMediaStreamTrack(ExecutionContext* execution_context,
                                 MediaStreamComponent* component,
                                 MediaStreamSource::ReadyState ready_state,
                                 base::OnceClosure callback);

  ~BrowserCaptureMediaStreamTrack() override = default;

  void Trace(Visitor*) const override;

  // Allows tests to invoke OnCaptureVersionObserved() directly, since
  // triggering it via mocks would be prohibitively difficult.
  void OnCaptureVersionObservedForTesting(
      media::CaptureVersion capture_version) {
    OnCaptureVersionObserved(capture_version);
  }

  ScriptPromise<IDLUndefined> cropTo(ScriptState*,
                                     CropTarget*,
                                     ExceptionState&);
  ScriptPromise<IDLUndefined> restrictTo(ScriptState*,
                                         RestrictionTarget*,
                                         ExceptionState&);

  BrowserCaptureMediaStreamTrack* clone(ExecutionContext*) override;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ApplySubCaptureTargetResult {
    kOk = 0,
    kTimedOut = 1,
    kInvalidFormat = 2,
    kRejectedWithErrorGeneric = 3,
    kRejectedWithUnsupportedCaptureDevice = 4,
    kRejectedWithNotImplemented = 5,
    kNonIncreasingVersion = 6,
    kInvalidTarget = 7,
    kUnsupportedPlatform = 8,
    kMaxValue = kUnsupportedPlatform
  };

 private:
  // Helper function serving cropTo(), restrictTo(), and any potential
  // future function that takes a BCMST and mutates what it is capturing
  // to some subset of the original target, based on a target identified
  // using a SubCaptureTarget.
  ScriptPromise<IDLUndefined> ApplySubCaptureTarget(ScriptState*,
                                                    SubCaptureTarget::Type,
                                                    SubCaptureTarget*,
                                                    ExceptionState&);

  struct PromiseInfo : GarbageCollected<PromiseInfo> {
    explicit PromiseInfo(
        ScriptPromiseResolverWithTracker<ApplySubCaptureTargetResult,
                                         IDLUndefined>* promise_resolver)
        : promise_resolver(promise_resolver) {}

    void Trace(Visitor* visitor) const { visitor->Trace(promise_resolver); }

    const Member<ScriptPromiseResolverWithTracker<ApplySubCaptureTargetResult,
                                                  IDLUndefined>>
        promise_resolver;
    std::optional<media::mojom::ApplySubCaptureTargetResult> result;
    bool capture_version_observed = false;
  };

  using SubCaptureVersionToPromiseInfoMap =
      HeapHashMap<media::CaptureVersion,
                  Member<PromiseInfo>,
                  TwoFieldsHashTraits<media::CaptureVersion,
                                      &media::CaptureVersion::source,
                                      &media::CaptureVersion::sub_capture>>;
  using PromiseMapIterator = SubCaptureVersionToPromiseInfoMap::iterator;

  // Each cropTo() or restrictTo() call is associated with a unique
  // `capture_version` which identifies this specific invocation.
  // When the browser process responds with the result of the invocation,
  // it triggers a call to OnResultFromBrowserProcess() with that
  // `capture_version`.
  void OnResultFromBrowserProcess(
      media::CaptureVersion capture_version,
      media::mojom::ApplySubCaptureTargetResult result);

  // OnCaptureVersionObserved() is posted as a callback, bound to a
  // unique `capture_version`. This callback be invoked when either:
  // - The first frame is observed which is associated with `capture_version`.
  // - A frame with an even newer `capture_version` is observed.
  // - A message is received guaranteeing that all subsequent frames will
  //   have `capture_version` or newer. (E.g. if cropping is applied correctly
  //   but there is no frame to produce yet.)
  void OnCaptureVersionObserved(media::CaptureVersion capture_version);

  // The Promise that cropTo() or restrictTo() issued are resolved when
  // both conditions are fulfulled:
  // 1. OnResultFromBrowserProcess(kSuccess) called.
  // 2. OnCaptureVersionObserved() called for the associated `capture_version`.
  //
  // The order of fulfillment does not matter.
  //
  // The Promise is rejected if OnResultFromBrowserProcess() is called with
  // an error value.
  void MaybeFinalizeSubCapturePromise(PromiseMapIterator iter);

  // Each time cropTo() is called on a given track, its sub-capture version
  // increments. Associate each Promise with its sub-capture version, so that
  // Viz can easily stamp each frame. When we see the first such frame,
  // or an equivalent message, we can resolve the Promise. (An "equivalent
  // message" can be a notification of a dropped frame, or a notification that
  // a frame was not produced due to consisting of 0 pixels after the crop was
  //  applied, or anything similar.)
  //
  // Note that frames before the first call to cropTo() will be associated
  // with a version of 0, both here and in Viz.
  SubCaptureVersionToPromiseInfoMap pending_promises_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_
