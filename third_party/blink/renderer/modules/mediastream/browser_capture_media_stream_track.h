// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/focusable_media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class MODULES_EXPORT BrowserCaptureMediaStreamTrack final
    : public FocusableMediaStreamTrack {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BrowserCaptureMediaStreamTrack(ExecutionContext* execution_context,
                                 MediaStreamComponent* component,
                                 base::OnceClosure callback,
                                 const String& descriptor_id,
                                 bool is_clone = false);

  BrowserCaptureMediaStreamTrack(ExecutionContext* execution_context,
                                 MediaStreamComponent* component,
                                 MediaStreamSource::ReadyState ready_state,
                                 base::OnceClosure callback,
                                 const String& descriptor_id,
                                 bool is_clone = false);

#if !BUILDFLAG(IS_ANDROID)
  void Trace(Visitor*) const override;
#endif

  ScriptPromise cropTo(ScriptState*, const String&, ExceptionState&);

  BrowserCaptureMediaStreamTrack* clone(ScriptState*) override;

 private:
  // Given a partially built MediaStreamTrack, finishes the job of making it
  // into a clone of |this|.
  // Useful for sub-classes (caveat below), as they need to clone both state
  // from this class as well as of their own class.
  // Caveat: This class is final, and has no sub-classes. We continue the
  // pattern from the parent classes for clarity, and to make things easier
  // if we do in the future sub-class further.
  void CloneInternal(BrowserCaptureMediaStreamTrack*);

#if !BUILDFLAG(IS_ANDROID)
  // Resolves the Promise associated with |crop_version|.
  void ResolveCropPromise(uint32_t crop_version,
                          media::mojom::CropRequestResult result);

  // Each time cropTo() is called on a given track, its crop version increments.
  // Associate each Promise with its crop version, so that Viz can easily stamp
  // each frame. When we see the first such frame, or an equivalent message,
  // we can resolve the Promise. (An "equivalent message" can be a notification
  // of a dropped frame, or a notification that a frame was not produced due
  // to consisting of 0 pixels after the crop was applied, or anything similar.)
  //
  // Note that frames before the first call to cropTo() will be associated
  // with a version of 0, both here and in Viz.
  HeapHashMap<uint32_t, Member<ScriptPromiseResolver>> pending_promises_;
  uint32_t current_crop_version_ = 0;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_BROWSER_CAPTURE_MEDIA_STREAM_TRACK_H_
