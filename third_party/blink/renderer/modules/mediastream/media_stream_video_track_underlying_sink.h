// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_

#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MediaStreamVideoSource;
class PushableMediaStreamVideoSource;

class MODULES_EXPORT MediaStreamVideoTrackUnderlyingSink
    : public UnderlyingSinkBase {
 public:
  // |source| must outlive this MediaStreamVideoTrackUnderlyingSink.
  explicit MediaStreamVideoTrackUnderlyingSink(
      PushableMediaStreamVideoSource* source);

  // UnderlyingSinkBase overrides.
  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override;
  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override;
  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState& exception_state) override;
  ScriptPromise close(ScriptState* script_state,
                      ExceptionState& exception_state) override;

 private:
  base::WeakPtr<MediaStreamVideoSource> source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_
