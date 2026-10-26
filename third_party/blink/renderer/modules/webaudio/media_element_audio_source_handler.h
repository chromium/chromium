// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_HANDLER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/audio/media_multi_channel_resampler.h"

namespace blink {

class AudioContext;
class HTMLMediaElement;
class KURL;
class MediaElementAudioSourceOptions;
class WebAudioSourceProviderImpl;

class MODULES_EXPORT MediaElementAudioSourceHandler final
    : public AudioHandler {
 public:
  static scoped_refptr<MediaElementAudioSourceHandler> Create(
      AudioNode&,
      HTMLMediaElement&);
  ~MediaElementAudioSourceHandler() override;

  // AudioHandler
  void Dispose() override;
  void Process(uint32_t frames_to_process) override;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

  // Helpers for AudioSourceProviderClient implementation of
  // MediaElementAudioSourceNode.
  void SetFormat(uint32_t number_of_channels, float sample_rate);
  void lock() EXCLUSIVE_LOCK_FUNCTION(GetProcessLock());
  void unlock() UNLOCK_FUNCTION(GetProcessLock());
  void OnCurrentSrcChanged(const KURL& current_src);

  unsigned SourceNumberOfChannelsForTesting();
  double SourceSampleRateForTesting();
  bool IsOriginTaintedForTesting();
  bool WouldTaintOriginForTesting();
  bool HasAudioSourceProviderForTesting();
  void SetAudioSourceProviderForTesting(
      scoped_refptr<WebAudioSourceProviderImpl> provider);
  void ProvideResamplerInputForTesting(int resampler_frame_delay,
                                       AudioBus* dest);

  // For thread safety analysis only.  Does not actually return mu.
  base::Lock* GetProcessLock() LOCK_RETURNED(process_lock_) { NOTREACHED(); }

  bool RequiresTailProcessing() const final { return false; }

 private:
  MediaElementAudioSourceHandler(AudioNode&, HTMLMediaElement&);
  // As an audio source, we will never propagate silence.
  bool PropagatesSilence() const override { return false; }

  HTMLMediaElement* GetMediaElement();

  // Returns true if the origin of the media element is tainted so that the
  // audio should be muted when playing through WebAudio.
  bool WouldTaintOrigin();

  // Print warning if CORS restrictions cause MediaElementAudioSource to output
  // zeroes.
  void PrintCorsMessage(const String& message);

  // Pulls rendered audio frames from `audio_source_provider_` into `bus`.
  void ProvideInput(AudioBus* bus, int frames_to_process)
      EXCLUSIVE_LOCKS_REQUIRED(process_lock_);

  // Provide input to the resampler (if used).
  void ProvideResamplerInput(int resampler_frame_delay, AudioBus* dest)
      EXCLUSIVE_LOCKS_REQUIRED(process_lock_);

  base::Lock process_lock_;

  scoped_refptr<WebAudioSourceProviderImpl> audio_source_provider_
      GUARDED_BY(process_lock_);

  unsigned source_number_of_channels_ GUARDED_BY(process_lock_) = 0;
  double source_sample_rate_ GUARDED_BY(process_lock_) = 0;

  std::unique_ptr<MediaMultiChannelResampler> multi_channel_resampler_
      GUARDED_BY(process_lock_);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // True if the origin would be tainted by the media element.  In this case,
  // this node outputs silence.  This can happen if the media element source is
  // a cross-origin source which we're not allowed to access due to CORS
  // restrictions.
  bool is_origin_tainted_ GUARDED_BY(process_lock_) = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_HANDLER_H_
