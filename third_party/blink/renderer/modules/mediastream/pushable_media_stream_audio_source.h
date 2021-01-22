// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PUSHABLE_MEDIA_STREAM_AUDIO_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PUSHABLE_MEDIA_STREAM_AUDIO_SOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class AudioFrameSerializationData;

// Simplifies the creation of audio tracks.
class MODULES_EXPORT PushableMediaStreamAudioSource
    : public MediaStreamAudioSource {
 public:
  PushableMediaStreamAudioSource(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SequencedTaskRunner> audio_task_runner);

  ~PushableMediaStreamAudioSource() override;

  // This can be called from any thread, and will push the data on
  // |audio_task_runner_|
  void PushAudioData(std::unique_ptr<AudioFrameSerializationData> data);

  bool running() const {
    DCHECK(GetTaskRunner()->BelongsToCurrentThread());
    return is_running_;
  }

 private:
  // Helper class that facilitates the jump to |audio_task_runner_|, by
  // outliving the given |source| if there is a pending task.
  class LivenessBroker : public WTF::ThreadSafeRefCounted<LivenessBroker> {
   public:
    explicit LivenessBroker(PushableMediaStreamAudioSource* source);

    void OnSourceDestroyedOrStopped();
    void PushAudioData(std::unique_ptr<AudioFrameSerializationData> data);

   private:
    WTF::Mutex mutex_;
    PushableMediaStreamAudioSource* source_ GUARDED_BY(mutex_);
  };

  // Actually push data to the audio tracks. Only called on
  // |audio_task_runner_|.
  void DeliverData(std::unique_ptr<AudioFrameSerializationData> data);

  // MediaStreamAudioSource implementation.
  bool EnsureSourceIsStarted() final;
  void EnsureSourceIsStopped() final;

  bool is_running_ = false;

  int last_channels_ = 0;
  int last_frames_ = 0;
  int last_sample_rate_ = 0;

  scoped_refptr<base::SequencedTaskRunner> audio_task_runner_;
  scoped_refptr<LivenessBroker> liveness_broker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PUSHABLE_MEDIA_STREAM_AUDIO_SOURCE_H_
