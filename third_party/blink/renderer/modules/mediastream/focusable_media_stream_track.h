// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FOCUSABLE_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FOCUSABLE_MEDIA_STREAM_TRACK_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_start_focus_behavior.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT FocusableMediaStreamTrack : public MediaStreamTrackImpl {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FocusableMediaStreamTrack(ExecutionContext* execution_context,
                            MediaStreamComponent* component,
                            base::OnceClosure callback,
                            const String& descriptor_id,
                            bool is_clone = false);

  FocusableMediaStreamTrack(ExecutionContext* execution_context,
                            MediaStreamComponent* component,
                            MediaStreamSource::ReadyState ready_state,
                            base::OnceClosure callback,
                            const String& descriptor_id,
                            bool is_clone = false);

#if !BUILDFLAG(IS_ANDROID)
  void CloseFocusWindowOfOpportunity() override;
#endif

  void focus(ExecutionContext* execution_context,
             V8CaptureStartFocusBehavior focus_behavior,
             ExceptionState& exception_state);

  // Clones raise an error if focus() is called.
  FocusableMediaStreamTrack* clone(ScriptState*) override;

 protected:
  // Given a partially built FocusableMediaStreamTrack, finishes the job
  // of making it into a clone of |this|.
  // Useful for sub-classes, as they need to clone both state from
  // this class as well as of their own class.
  void CloneInternal(FocusableMediaStreamTrack*);

  const String& descriptor_id() const { return descriptor_id_; }

 private:
#if !BUILDFLAG(IS_ANDROID)
  // Clones may not be focus()-ed.
  const bool is_clone_;

  // Calling focus() after the microtask on which getDisplayMedia()'s Promise
  // was settled raises an exception.
  bool promise_settled_ = false;

  // First call to focus() is allowed. Subsequent calls produce an error.
  bool focus_called_ = false;
#endif

  // On the browser-side, this track is associated with this ID.
  // It is known as the "label" in the dispatcher and in MediaStreamManager.
  const String descriptor_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FOCUSABLE_MEDIA_STREAM_TRACK_H_
