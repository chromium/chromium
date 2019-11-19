// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/audio/audio_debug_recording_helper.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManager;
class AudioOutputDispatcherImpl;
class OnMoreDataConverter;

// AudioOutputResampler is a browser-side resampling and buffering solution
// which ensures audio data is always output at given parameters.  See the
// AudioConverter class for details on the conversion process.
//
// AOR works by intercepting the AudioSourceCallback provided to StartStream()
// and redirecting it through an AudioConverter instance.
//
// AOR will automatically fall back from AUDIO_PCM_LOW_LATENCY to
// AUDIO_PCM_LINEAR if the output device fails to open at the requested output
// parameters. If opening still fails, it will fallback to AUDIO_FAKE.
class MEDIA_EXPORT AudioOutputResampler : public AudioOutputDispatcher {
 public:
  // Callback type to register an AudioDebugRecorder.
  using RegisterDebugRecordingSourceCallback =
      base::RepeatingCallback<std::unique_ptr<AudioDebugRecorder>(
          const AudioParameters&)>;

  AudioOutputResampler(AudioManager* audio_manager,
                       const AudioParameters& input_params,
                       const AudioParameters& output_params,
                       const std::string& output_device_id,
                       base::TimeDelta close_delay,
                       const RegisterDebugRecordingSourceCallback&
                           register_debug_recording_source_callback);
  ~AudioOutputResampler() override;

  // AudioOutputDispatcher interface.
  AudioOutputProxy* CreateStreamProxy() override;
  bool OpenStream() override;
  bool StartStream(AudioOutputStream::AudioSourceCallback* callback,
                   AudioOutputProxy* stream_proxy) override;
  void StopStream(AudioOutputProxy* stream_proxy) override;
  void StreamVolumeSet(AudioOutputProxy* stream_proxy, double volume) override;
  void CloseStream(AudioOutputProxy* stream_proxy) override;
  void FlushStream(AudioOutputProxy* stream_proxy) override;

 private:
  using CallbackMap =
      base::flat_map<AudioOutputProxy*, std::unique_ptr<OnMoreDataConverter>>;

  // Used to reinitialize |dispatcher_| upon timeout if there are no open
  // streams.
  void Reinitialize();

  // Used to initialize |dispatcher_|.
  std::unique_ptr<AudioOutputDispatcherImpl> MakeDispatcher(
      const std::string& output_device_id,
      const AudioParameters& params);

  // Stops the stream corresponding to the |item| in |callbacks_|.
  void StopStreamInternal(const CallbackMap::value_type& item);

  // Dispatcher to proxy all AudioOutputDispatcher calls too.
  // Lazily initialized on a first stream open request.
  std::unique_ptr<AudioOutputDispatcherImpl> dispatcher_;

  // Map of outstanding OnMoreDataConverter objects.  A new object is created
  // on every StartStream() call and destroyed on CloseStream().
  CallbackMap callbacks_;

  // Used by AudioOutputDispatcherImpl; kept so we can reinitialize on the fly.
  const base::TimeDelta close_delay_;

  // Source AudioParameters.
  const AudioParameters input_params_;

  // AudioParameters used to setup the output stream; changed upon fallback.
  AudioParameters output_params_;

  // The original AudioParameters we were constructed with.
  const AudioParameters original_output_params_;

  // Output device id.
  const std::string device_id_;

  // The reinitialization timer provides a way to recover from temporary failure
  // states by clearing the dispatcher if all proxies have been closed and none
  // have been created within |close_delay_|.  Without this, audio may be lost
  // to a fake stream indefinitely for transient errors.
  base::RetainingOneShotTimer reinitialize_timer_;

  // Callback for registering a debug recording source.
  RegisterDebugRecordingSourceCallback
      register_debug_recording_source_callback_;

  base::WeakPtrFactory<AudioOutputResampler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AudioOutputResampler);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
