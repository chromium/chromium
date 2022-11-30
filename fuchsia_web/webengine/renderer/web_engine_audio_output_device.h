// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_OUTPUT_DEVICE_H_
#define FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_OUTPUT_DEVICE_H_

#include <fuchsia/media/cpp/fidl.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "media/base/audio_renderer_sink.h"

namespace base {
class SingleThreadTaskRunner;
class WritableSharedMemoryMapping;
}  // namespace base

// AudioRendererSink implementation for WebEngine. It sends audio to
// AudioConsumer provided by the OS. Unlike AudioOutputDevice this class sends
// to the system directly from the renderer process. All work is performed on
// the TaskRunner passed to Create(). It must be an IO thread to allow FIDL
// usage. AudioRendererSink can be used on a different thread.
class WEB_ENGINE_EXPORT WebEngineAudioOutputDevice
    : public media::AudioRendererSink {
 public:
  static scoped_refptr<WebEngineAudioOutputDevice> Create(
      fidl::InterfaceHandle<fuchsia::media::AudioConsumer>
          audio_consumer_handle,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Same as above, but creates a WebEngineAudioOutputDevice that runs on the
  // default audio thread.
  static scoped_refptr<WebEngineAudioOutputDevice> CreateOnDefaultThread(
      fidl::InterfaceHandle<fuchsia::media::AudioConsumer>
          audio_consumer_handle);

  // AudioRendererSink implementation.
  void Initialize(const media::AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Pause() override;
  void Play() override;
  void Flush() override;
  bool SetVolume(double volume) override;
  media::OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;

 private:
  friend class WebEngineAudioOutputDeviceTest;

  explicit WebEngineAudioOutputDevice(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebEngineAudioOutputDevice() override;

  void BindAudioConsumerOnAudioThread(
      fidl::InterfaceHandle<fuchsia::media::AudioConsumer>
          audio_consumer_handle);

  // AudioRendererSink handlers for the audio thread.
  void InitializeOnAudioThread(const media::AudioParameters& params);
  void StartOnAudioThread();
  void StopOnAudioThread();
  void PauseOnAudioThread();
  void PlayOnAudioThread();
  void FlushOnAudioThread();
  void SetVolumeOnAudioThread(double volume);

  // Initializes |stream_sink_|.
  void CreateStreamSink();

  // Sends current volume to |volume_control_|.
  void UpdateVolume();

  // Polls current |audio_consumer_| status.
  void WatchAudioConsumerStatus();

  // Callback for AudioConsumer::WatchStatus().
  void OnAudioConsumerStatusChanged(fuchsia::media::AudioConsumerStatus status);

  // Schedules next PumpSamples() to pump next audio packet.
  void SchedulePumpSamples();

  // Pumps a single packet to AudioConsumer and calls SchedulePumpSamples() to
  // pump the next packet.
  void PumpSamples(base::TimeTicks playback_time);

  // Callback for StreamSink::SendPacket().
  void OnStreamSendDone(size_t buffer_index);

  // Reports an error and shuts down the AudioConsumer connection.
  void ReportError();

  // Task runner used for all activity. Normally this is the audio thread owned
  // by FuchsiaAudioDeviceFactory. All AudioRendererSink methods are called on
  // another thread (normally the main renderer thread on which this object is
  // created).
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  fuchsia::media::AudioConsumerPtr audio_consumer_;
  fuchsia::media::StreamSinkPtr stream_sink_;
  fuchsia::media::audio::VolumeControlPtr volume_control_;

  media::AudioParameters params_;

  // Lock used to control access to |callback_|.
  base::Lock callback_lock_;

  // Callback passed to Initialize(). It's set on the main thread (that calls
  // Initialize() and Stop()), but used on the audio thread (which corresponds
  // to the |task_runner_|). This is necessary because AudioRendererSink must
  // guarantee that the callback is not called after Stop(). |callback_lock_| is
  // used to synchronize access to the |callback_|.
  RenderCallback* callback_ GUARDED_BY(callback_lock_) = nullptr;

  // Mapped memory for buffers shared with |stream_sink_|.
  std::vector<base::WritableSharedMemoryMapping> stream_sink_buffers_;

  // Indices of unused buffers in |stream_sink_buffers_|.
  std::vector<size_t> available_buffers_indices_;

  float volume_ = 1.0;

  // Current position in the stream in samples since the stream was started.
  size_t media_pos_frames_ = 0;

  // Current minimum lead time returned by the |audio_consumer_|.
  base::TimeDelta min_lead_time_;

  // Current timeline parameters provided by the |audio_consumer_| in the last
  // AudioConsumerStatus. See
  // https://fuchsia.dev/reference/fidl/fuchsia.media#TimelineFunction for
  // details on how these parameters are used. |timeline_reference_time_| is set
  // to null value when there is no presentation timeline (i.e. playback isn't
  // active).
  base::TimeTicks timeline_reference_time_;
  base::TimeDelta timeline_subject_time_;
  uint32_t timeline_reference_delta_;
  uint32_t timeline_subject_delta_;

  // Set to true between DoPause() and DoPlay(). AudioConsumer implementations
  // should drop |presentation_timeline| when the stream is paused, but the
  // state is updated asynchronously. This flag is used to avoid sending packets
  // until the state is updated.
  bool paused_ = false;

  // Timer for PumpSamples().
  base::OneShotTimer pump_samples_timer_;

  // AudioBus used in PumpSamples(). Stored here to avoid re-allocating it for
  // every packet.
  std::unique_ptr<media::AudioBus> audio_bus_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_OUTPUT_DEVICE_H_
