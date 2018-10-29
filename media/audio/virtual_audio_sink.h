// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_VIRTUAL_AUDIO_SINK_H_
#define MEDIA_AUDIO_VIRTUAL_AUDIO_SINK_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_source_diverter.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_shifter.h"

namespace media {

class VirtualAudioInputStream;

// This class is an adapter between push model and pull model for audio stream.
// This class is thread safe.
// Audio provider calls OnData() to push audio data into this class.
// Audio consumer calls ProvideInput() to pull audio data.
class MEDIA_EXPORT VirtualAudioSink : public AudioPushSink,
                                      public AudioConverter::InputCallback {
 public:
  typedef base::Callback<void(VirtualAudioSink* sink)> AfterCloseCallback;

  // Construct an audio loopback pathway to the given |target| (not owned).
  // |target| must outlive this instance.
  VirtualAudioSink(const AudioParameters& param,
                   VirtualAudioInputStream* target,
                   const AfterCloseCallback& callback);
  ~VirtualAudioSink() override;

  // AudioPushSink
  void Close() override;
  void OnData(std::unique_ptr<AudioBus> source,
              base::TimeTicks reference_time) override;

  // AudioConverter::InputCallback
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override;

 private:
  void StoreData(AudioBus* source, base::TimeTicks reference_time);

  const AudioParameters params_;
  VirtualAudioInputStream* const target_;
  AudioShifter shifter_ GUARDED_BY(shifter_lock_);
  base::Lock shifter_lock_;
  AfterCloseCallback after_close_callback_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioSink);
};

}  // namespace media

#endif  // MEDIA_AUDIO_VIRTUAL_AUDIO_SINK_H_
