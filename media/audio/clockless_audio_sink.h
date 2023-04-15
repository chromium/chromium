// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CLOCKLESS_AUDIO_SINK_H_
#define MEDIA_AUDIO_CLOCKLESS_AUDIO_SINK_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "media/base/audio_hash.h"
#include "media/base/audio_renderer_sink.h"

namespace media {
class ClocklessAudioSinkThread;

// Implementation of an AudioRendererSink that consumes the audio as fast as
// possible. This class does not support multiple Play()/Pause() events.
class MEDIA_EXPORT ClocklessAudioSink : public AudioRendererSink {
 public:
  ClocklessAudioSink();
  explicit ClocklessAudioSink(const OutputDeviceInfo& device_info);

  ClocklessAudioSink(const ClocklessAudioSink&) = delete;
  ClocklessAudioSink& operator=(const ClocklessAudioSink&) = delete;

  // AudioRendererSink implementation.
  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Play() override;
  bool SetVolume(double volume) override;
  OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;

  // Returns the time taken to consume all the audio.
  base::TimeDelta render_time() { return playback_time_; }

  // Enables audio frame hashing.  Must be called prior to Initialize().
  void StartAudioHashForTesting();

  // Returns the hash of all audio frames seen since construction.
  const AudioHash& GetAudioHashForTesting() const;

  void SetIsOptimizedForHardwareParametersForTesting(bool value);

 protected:
  ~ClocklessAudioSink() override;

 private:
  const OutputDeviceInfo device_info_;
  std::unique_ptr<ClocklessAudioSinkThread> thread_;
  bool initialized_;
  bool playing_;
  bool hashing_;
  bool is_optimized_for_hw_params_;

  // Time taken in last set of Render() calls.
  base::TimeDelta playback_time_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_CLOCKLESS_AUDIO_SINK_H_
