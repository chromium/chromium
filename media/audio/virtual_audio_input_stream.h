// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "media/base/fake_audio_worker.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class LoopbackAudioConverter;

// VirtualAudioInputStream converts and mixes audio from attached
// VirtualAudioOutputStreams into a single stream. It will continuously render
// audio until this VirtualAudioInputStream is stopped and closed.
class MEDIA_EXPORT VirtualAudioInputStream : public AudioInputStream {
 public:
  // Callback invoked just after VirtualAudioInputStream is closed.
  typedef base::Callback<void(VirtualAudioInputStream* vais)>
      AfterCloseCallback;

  // Construct a target for audio loopback which mixes multiple data streams
  // into a single stream having the given |params|.  |worker_task_runner| is
  // the task runner on which AudioInputCallback methods are called and may or
  // may not be the single thread that invokes the AudioInputStream methods.
  VirtualAudioInputStream(
      const AudioParameters& params,
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner,
      const AfterCloseCallback& after_close_cb);

  ~VirtualAudioInputStream() override;

  // AudioInputStream:
  bool Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // Attaches an AudioConverter::InputCallback to be used as input. This
  // VirtualAudioInputStream must outlive all attached streams, so any attached
  // stream must be closed (which causes a detach) before
  // VirtualAudioInputStream is destroyed.
  virtual void AddInputProvider(AudioConverter::InputCallback* input,
                                const AudioParameters& params);

  // Detaches an AudioConverter::InputCallback and removes it as input.
  virtual void RemoveInputProvider(AudioConverter::InputCallback* input,
                                   const AudioParameters& params);

 private:
  friend class VirtualAudioInputStreamTest;

  typedef std::map<AudioParameters, LoopbackAudioConverter*> AudioConvertersMap;

  // Pulls audio data from all attached VirtualAudioOutputStreams, mixes and
  // converts the streams into one, and pushes the result to |callback_|.
  // Invoked on the worker thread.
  void PumpAudio();

  const scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  AfterCloseCallback after_close_cb_;

  AudioInputCallback* callback_;

  // Non-const for testing.
  AudioParameters params_;

  // Guards concurrent access to the converter network: converters_, mixer_, and
  // num_attached_output_streams_.
  base::Lock converter_network_lock_;

  // AudioConverters associated with the attached VirtualAudioOutputStreams,
  // partitioned by common AudioParameters.
  AudioConvertersMap converters_ GUARDED_BY(converter_network_lock_);

  // AudioConverter that takes all the audio converters and mixes them into one
  // final audio stream.
  AudioConverter mixer_ GUARDED_BY(converter_network_lock_);

  // Number of currently attached VirtualAudioOutputStreams.
  int num_attached_output_streams_ GUARDED_BY(converter_network_lock_);

  // Handles callback timing for consumption of audio data.
  FakeAudioWorker fake_worker_;

  std::unique_ptr<AudioBus> audio_bus_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_
