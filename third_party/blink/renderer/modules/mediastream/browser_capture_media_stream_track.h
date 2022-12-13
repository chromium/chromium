// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver_with_tracker.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

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

#if !BUILDFLAG(IS_ANDROID)
  void Trace(Visitor*) const override;

  // Allows tests to invoke OnCropVersionObserved() directly, since triggering
  // it via mocks would be prohibitively difficult.
  void OnCropVersionObservedForTesting(uint32_t crop_version) {
    OnCropVersionObserved(crop_version);
  }
#endif

  ScriptPromise cropTo(ScriptState*, CropTarget*, ExceptionState&);

  BrowserCaptureMediaStreamTrack* clone(ExecutionContext*) override;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CropToResult {
    kOk = 0,
    kUnsupportedPlatform = 1,
    kInvalidCropTargetFormat = 2,
    kRejectedWithErrorGeneric = 3,
    kRejectedWithUnsupportedCaptureDevice = 4,
    kRejectedWithErrorUnknownDeviceId_DEPRECATED = 5,
    kRejectedWithNotImplemented = 6,
    kNonIncreasingCropVersion = 7,
    kInvalidCropTarget = 8,
    kTimedOut = 9,
    kMaxValue = kTimedOut
  };

 private:
#if !BUILDFLAG(IS_ANDROID)
  struct CropPromiseInfo : GarbageCollected<CropPromiseInfo> {
    explicit CropPromiseInfo(
        ScriptPromiseResolverWithTracker<CropToResult>* promise_resolver)
        : promise_resolver(promise_resolver) {}

    void Trace(Visitor* visitor) const { visitor->Trace(promise_resolver); }

    const Member<ScriptPromiseResolverWithTracker<CropToResult>>
        promise_resolver;
    absl::optional<media::mojom::CropRequestResult> crop_result;
    bool crop_version_observed = false;
  };

  using CropVersionToPromiseInfoMap =
      HeapHashMap<uint32_t,
                  Member<BrowserCaptureMediaStreamTrack::CropPromiseInfo>>;
  using PromiseMapIterator = CropVersionToPromiseInfoMap::iterator;

  // Each cropTo() call is associated with a unique |crop_version| which
  // identifies this specific cropTo() invocation. When the browser process
  // responds with the result of the cropTo() invocation, it triggers
  // a call to OnResultFromBrowserProcess() with that |crop_version|.
  void OnResultFromBrowserProcess(uint32_t crop_version,
                                  media::mojom::CropRequestResult result);

  // OnCropVersionObserved() is posted as a callback, bound to a unique
  // |crop_version|. This callback be invoked when the first frame is observed
  // which is associated with that |crop_version|.
  // TODO(crbug.com/1266378): The Promise should also be resolved if a
  // a barrier event is observed. (That is, although no frame is delivered,
  // there is a guarantee that all future frames will be of this version
  // or later. This would happen if cropping a muted track, for instance.)
  void OnCropVersionObserved(uint32_t crop_version);

  // The Promise that cropTo() issued is resolved when both conditions
  // are fulfulled:
  // 1. OnResultFromBrowserProcess(kSuccess) called.
  // 2. OnCropVersionObserved() called for the associated |crop_version|.
  //
  // The order of fulfillment does not matter.
  //
  // The Promise is rejected if OnResultFromBrowserProcess() is called with
  // an error value.
  void MaybeFinalizeCropPromise(PromiseMapIterator iter);

  // Each time cropTo() is called on a given track, its crop version increments.
  // Associate each Promise with its crop version, so that Viz can easily stamp
  // each frame. When we see the first such frame, or an equivalent message,
  // we can resolve the Promise. (An "equivalent message" can be a notification
  // of a dropped frame, or a notification that a frame was not produced due
  // to consisting of 0 pixels after the crop was applied, or anything similar.)
  //
  // Note that frames before the first call to cropTo() will be associated
  // with a version of 0, both here and in Viz.
  HeapHashMap<uint32_t, Member<CropPromiseInfo>> pending_promises_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_
