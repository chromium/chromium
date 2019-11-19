// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PROCESSED_LOCAL_AUDIO_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PROCESSED_LOCAL_AUDIO_SOURCE_H_

#include <string>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_capturer_source.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_level_calculator.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace media {
class AudioBus;
class AudioProcessorControls;
}  // namespace media

namespace blink {

class AudioServiceAudioProcessorProxy;
class MediaStreamAudioProcessor;
class MediaStreamInternalFrameWrapper;
class WebLocalFrame;

// Represents a local source of audio data that is routed through the WebRTC
// audio pipeline for post-processing (e.g., for echo cancellation during a
// video conferencing call). Owns a media::AudioCapturerSource and the
// MediaStreamProcessor that modifies its audio. Modified audio is delivered to
// one or more MediaStreamAudioTracks.
class MODULES_EXPORT ProcessedLocalAudioSource final
    : public blink::MediaStreamAudioSource,
      public media::AudioCapturerSource::CaptureCallback {
 public:
  // |internal_consumer_frame_| references the blink::LocalFrame that will
  // consume the audio data. Audio parameters and (optionally) a pre-existing
  // audio session ID are derived from |device_info|. |factory| must outlive
  // this instance.
  ProcessedLocalAudioSource(
      WebLocalFrame* web_frame,
      const blink::MediaStreamDevice& device,
      bool disable_local_echo,
      const blink::AudioProcessingProperties& audio_processing_properties,
      ConstraintsOnceCallback started_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~ProcessedLocalAudioSource() final;

  // If |source| is an instance of ProcessedLocalAudioSource, return a
  // type-casted pointer to it. Otherwise, return null.
  static ProcessedLocalAudioSource* From(blink::MediaStreamAudioSource* source);

  // Non-browser unit tests cannot provide RenderFrame implementations at
  // run-time. This is used to skip the otherwise mandatory check for a valid
  // render frame ID when the source is started.
  void SetAllowInvalidRenderFrameIdForTesting(bool allowed) {
    allow_invalid_render_frame_id_for_testing_ = allowed;
  }

  const blink::AudioProcessingProperties& audio_processing_properties() const {
    return audio_processing_properties_;
  }

  base::Optional<blink::AudioProcessingProperties>
  GetAudioProcessingProperties() const final;

  // The following accessors are valid after the source is started (when the
  // first track is connected).
  scoped_refptr<webrtc::AudioProcessorInterface> GetAudioProcessor() const;

  bool HasAudioProcessing() const;

  const scoped_refptr<blink::MediaStreamAudioLevelCalculator::Level>&
  audio_level() const {
    return level_calculator_.level();
  }

  // Thread-safe volume accessors used by WebRtcAudioDeviceImpl.
  void SetVolume(int volume);
  int Volume() const;
  int MaxVolume() const;

  void SetOutputDeviceForAec(const std::string& output_device_id);

 protected:
  // MediaStreamAudioSource implementation.
  void* GetClassIdentifier() const final;
  bool EnsureSourceIsStarted() final;
  void EnsureSourceIsStopped() final;

  // AudioCapturerSource::CaptureCallback implementation.
  // Called on the AudioCapturerSource audio thread.
  void OnCaptureStarted() override;
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override;
  void OnCaptureError(const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;
  void OnCaptureProcessorCreated(
      media::AudioProcessorControls* controls) override;

 private:
  // Runs the audio through |audio_processor_| before sending it along.
  void CaptureUsingProcessor(const media::AudioBus* audio_source,
                             base::TimeTicks audio_capture_time,
                             double volume,
                             bool key_pressed);

  // Helper function to get the source buffer size based on whether audio
  // processing will take place.
  int GetBufferSize(int sample_rate) const;

  // The LocalFrame that will consume the audio data. Used when creating
  // AudioCapturerSources.
  std::unique_ptr<MediaStreamInternalFrameWrapper> internal_consumer_frame_;

  blink::AudioProcessingProperties audio_processing_properties_;

  // Callback that's called when the audio source has been initialized.
  ConstraintsOnceCallback started_callback_;

  // At most one of |audio_processor_| and |audio_processor_proxy_| can be set.

  // Audio processor doing processing like FIFO, AGC, AEC and NS. Its output
  // data is in a unit of 10 ms data chunk.
  scoped_refptr<MediaStreamAudioProcessor> audio_processor_;

  // Proxy for the audio processor when it's run in the Audio Service process,
  scoped_refptr<AudioServiceAudioProcessorProxy> audio_processor_proxy_;

  // The device created by the AudioDeviceFactory in EnsureSourceIsStarted().
  scoped_refptr<media::AudioCapturerSource> source_;

  // Stores latest microphone volume received in a CaptureData() callback.
  // Range is [0, 255].
  base::subtle::Atomic32 volume_;

  // Used to calculate the signal level that shows in the UI.
  blink::MediaStreamAudioLevelCalculator level_calculator_;

  bool allow_invalid_render_frame_id_for_testing_;

  // Provides weak pointers for tasks posted by this instance.
  base::WeakPtrFactory<ProcessedLocalAudioSource> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProcessedLocalAudioSource);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PROCESSED_LOCAL_AUDIO_SOURCE_H_
