// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FOCUSABLE_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FOCUSABLE_MEDIA_STREAM_TRACK_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_start_focus_behavior.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

namespace blink {

class FocusableMediaStreamTrack final : public MediaStreamTrack {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FocusableMediaStreamTrack(ExecutionContext* execution_context,
                            MediaStreamComponent* component,
                            base::OnceClosure callback,
                            const String& descriptor_id);

  // Clones do not expose focus().
  MediaStreamTrack* clone(ScriptState*) override;

  void focus(ExecutionContext* execution_context,
             V8CaptureStartFocusBehavior focus_behavior,
             ExceptionState& exception_state);

 private:
  // On the browser-side, this track is associated with this ID.
  // It is known as the "label" in the dispatcher and in MediaStreamManager.
  const String descriptor_id_;

#if !defined(OS_ANDROID)
  // First call to focus() is allowed. Subsequent calls produce an error.
  bool focus_called_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FOCUSABLE_MEDIA_STREAM_TRACK_H_
