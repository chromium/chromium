// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_NULL_AUDIO_SINK_H_
#define MEDIA_AUDIO_NULL_AUDIO_SINK_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "media/base/audio_renderer_sink.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class AudioBus;
class AudioHash;
class FakeAudioWorker;

class MEDIA_EXPORT NullAudioSink : public SwitchableAudioRendererSink {
 public:
  explicit NullAudioSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // AudioRendererSink implementation.
  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Pause() override;
  void Play() override;
  void Flush() override;
  bool SetVolume(double volume) override;
  OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;
  void SwitchOutputDevice(const std::string& device_id,
                          OutputDeviceStatusCB callback) override;

  // Enables audio frame hashing.  Must be called prior to Initialize().
  void StartAudioHashForTesting();

  // Returns the hash of all audio frames seen since construction.
  std::string GetAudioHashForTesting();

 protected:
  ~NullAudioSink() override;

 private:
  // Task that periodically calls Render() to consume audio data.
  void CallRender(base::TimeTicks ideal_time, base::TimeTicks now);

  bool initialized_;
  bool started_;
  bool playing_;
  RenderCallback* callback_;

  // Controls whether or not a running hash is computed for audio frames.
  std::unique_ptr<AudioHash> audio_hash_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<FakeAudioWorker> fake_worker_;
  base::TimeDelta fixed_data_delay_;
  std::unique_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioSink);
};

}  // namespace media

#endif  // MEDIA_AUDIO_NULL_AUDIO_SINK_H_
