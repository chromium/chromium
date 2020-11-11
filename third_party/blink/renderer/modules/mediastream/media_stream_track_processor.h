// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_PROCESSOR_H_

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/impl/heap.h"

namespace blink {

class MediaStreamComponent;
class MediaStreamVideoTrackUnderlyingSource;
class ScriptState;
class ReadableStream;

class MODULES_EXPORT MediaStreamTrackProcessor : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaStreamTrackProcessor* Create(ScriptState*,
                                           MediaStreamTrack*,
                                           ExceptionState&);
  MediaStreamTrackProcessor(ScriptState*, MediaStreamComponent*);
  MediaStreamTrackProcessor(const MediaStreamTrackProcessor&) = delete;
  MediaStreamTrackProcessor& operator=(const MediaStreamTrackProcessor&) =
      delete;

  // MediaStreamTrackProcessor interface
  ReadableStream* readable(ScriptState* script_state);

  MediaStreamComponent* input_track() { return input_track_; }

  void Trace(Visitor* visitor) const override;

 private:
  void CreateVideoSourceStream(ScriptState* script_state);

  Member<MediaStreamComponent> input_track_;
  Member<MediaStreamVideoTrackUnderlyingSource> video_underlying_source_;
  Member<ReadableStream> source_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_PROCESSOR_H_
