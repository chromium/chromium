// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_TRACK_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_TRACK_UNDERLYING_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class AudioFrameSerializationData;
class MediaStreamComponent;

class MODULES_EXPORT MediaStreamAudioTrackUnderlyingSource
    : public UnderlyingSourceBase,
      public WebMediaStreamAudioSink {
 public:
  explicit MediaStreamAudioTrackUnderlyingSource(ScriptState*,
                                                 MediaStreamComponent*,
                                                 wtf_size_t queue_size);
  MediaStreamAudioTrackUnderlyingSource(
      const MediaStreamAudioTrackUnderlyingSource&) = delete;
  MediaStreamAudioTrackUnderlyingSource& operator=(
      const MediaStreamAudioTrackUnderlyingSource&) = delete;

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Start(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;

  // WebMediaStreamAudioSink
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override;
  void OnSetFormat(const media::AudioParameters& params) override;

  // ExecutionLifecycleObserver
  void ContextDestroyed() override;

  MediaStreamComponent* Track() const { return track_.Get(); }
  wtf_size_t MaxQueueSize() const { return max_queue_size_; }

  const Deque<std::unique_ptr<AudioFrameSerializationData>>& QueueForTesting()
      const {
    return queue_;
  }
  bool IsPendingPullForTesting() const { return is_pending_pull_; }

  double DesiredSizeForTesting() const;

  void Close();
  void Trace(Visitor*) const override;

 private:
  void ProcessPullRequest();
  void SendFrameToStream(std::unique_ptr<AudioFrameSerializationData>);

  void DisconnectFromTrack();

  void OnDataOnMainThread(std::unique_ptr<AudioFrameSerializationData> data);

  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  Member<MediaStreamComponent> track_;

  media::AudioParameters audio_parameters_;

  // An internal deque prior to the stream controller's queue. It acts as a ring
  // buffer and allows dropping old frames instead of new ones in case frames
  // accumulate due to slow consumption.
  Deque<std::unique_ptr<AudioFrameSerializationData>> queue_;
  const wtf_size_t max_queue_size_;
  bool is_pending_pull_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_TRACK_UNDERLYING_SOURCE_H_
