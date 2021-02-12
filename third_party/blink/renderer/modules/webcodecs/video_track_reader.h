// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_TRACK_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_TRACK_READER_H_

#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_output_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptState;

// Note: This class is deprecated. Use MediaStreamTrackProcessor instead.
// TODO(crbug.com/1157610): remove this class.
class MODULES_EXPORT VideoTrackReader final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver,
      public MediaStreamVideoSink {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoTrackReader* Create(ScriptState*,
                                  MediaStreamTrack*,
                                  ExceptionState&);
  VideoTrackReader(ScriptState*, MediaStreamTrack*);

  // Connects |this| to |track_| and starts delivering frames via |callback_|.
  void start(V8VideoFrameOutputCallback*, ExceptionState&);

  // Disconnects from |track_| and clears |callback_|.
  void stop(ExceptionState&);

  void Trace(Visitor* visitor) const override;

 private:
  VideoTrackReader(const VideoTrackReader&) = delete;
  VideoTrackReader& operator=(const VideoTrackReader&) = delete;

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override { DisconnectFromTrack(); }

  // MediaStreamVideoSink implementation.
  void OnReadyStateChanged(WebMediaStreamSource::ReadyState) override;

  // Callback For MediaStreamVideoSink::ConnectToTrack.
  void OnFrameFromVideoTrack(
      scoped_refptr<media::VideoFrame> media_frame,
      std::vector<scoped_refptr<media::VideoFrame>> scaled_media_frames,
      base::TimeTicks estimated_capture_time);

  void StopInternal();

  void ExecuteCallbackOnMainThread(
      scoped_refptr<media::VideoFrame> media_frame,
      std::vector<scoped_refptr<media::VideoFrame>> scaled_media_frames);

  // Whether we are connected to |track_| and using |callback_| to deliver
  // frames.
  bool started_;

  const scoped_refptr<base::SingleThreadTaskRunner>
      real_time_media_task_runner_;
  Member<V8VideoFrameOutputCallback> callback_;
  Member<MediaStreamTrack> track_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_TRACK_READER_H_
