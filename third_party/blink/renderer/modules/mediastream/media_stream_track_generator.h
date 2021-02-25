// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_GENERATOR_H_

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class MediaStreamAudioTrackUnderlyingSink;
class MediaStreamTrackGeneratorInit;
class MediaStreamVideoTrackUnderlyingSink;
class PushableMediaStreamVideoSource;
class ReadableStream;
class ScriptState;
class UnderlyingSourceBase;
class WritableStream;

class MODULES_EXPORT MediaStreamTrackGenerator : public MediaStreamTrack {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaStreamTrackGenerator* Create(ScriptState*,
                                           const String& kind,
                                           ExceptionState&);
  static MediaStreamTrackGenerator* Create(ScriptState*,
                                           MediaStreamTrackGeneratorInit* init,
                                           ExceptionState&);
  MediaStreamTrackGenerator(ScriptState*,
                            MediaStreamSource::StreamType,
                            const String& track_id,
                            MediaStreamTrack* signal_target,
                            wtf_size_t max_signal_buffer_size);
  MediaStreamTrackGenerator(const MediaStreamTrackGenerator&) = delete;
  MediaStreamTrackGenerator& operator=(const MediaStreamTrackGenerator&) =
      delete;

  WritableStream* writable(ScriptState* script_state);
  ReadableStream* readableControl(ScriptState* script_state);

  PushableMediaStreamVideoSource* PushableVideoSource() const;

  void Trace(Visitor* visitor) const override;

 private:
  void CreateAudioOutputPlatformTrack();
  void CreateAudioStream(ScriptState* script_state);

  void CreateVideoOutputPlatformTrack(MediaStreamTrack* signal_target);
  void CreateVideoStream(ScriptState* script_state);

  void CreateAudioControlStream(ScriptState* script_state);
  void CreateVideoControlStream(ScriptState* script_state);

  Member<MediaStreamAudioTrackUnderlyingSink> audio_underlying_sink_;
  Member<MediaStreamVideoTrackUnderlyingSink> video_underlying_sink_;
  Member<WritableStream> writable_;
  Member<UnderlyingSourceBase> control_underlying_source_;
  Member<ReadableStream> readable_control_;
  const wtf_size_t max_signal_buffer_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_GENERATOR_H_
