/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_NODE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/audio/media_multi_channel_resampler.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class AudioContext;
class HTMLMediaElement;
class MediaElementAudioSourceOptions;

class MediaElementAudioSourceHandler final : public AudioHandler {
 public:
  static scoped_refptr<MediaElementAudioSourceHandler> Create(
      AudioNode&,
      HTMLMediaElement&);
  ~MediaElementAudioSourceHandler() override;

  CrossThreadPersistent<HTMLMediaElement> MediaElement() const;

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

  // For thread safety analysis only.  Does not actually return mu.
  Mutex* GetProcessLock() LOCK_RETURNED(process_lock_) {
    NOTREACHED();
    return nullptr;
  }

  bool RequiresTailProcessing() const final { return false; }

 private:
  MediaElementAudioSourceHandler(AudioNode&, HTMLMediaElement&);
  // As an audio source, we will never propagate silence.
  bool PropagatesSilence() const override { return false; }

  // Returns true if the origin of the media element is tainted so that the
  // audio should be muted when playing through WebAudio.
  bool WouldTaintOrigin();

  // Print warning if CORS restrictions cause MediaElementAudioSource to output
  // zeroes.
  void PrintCorsMessage(const String& message);

  // Provide input to the resampler (if used).
  void ProvideResamplerInput(int resampler_frame_delay, AudioBus* dest);

  // The HTMLMediaElement is held alive by MediaElementAudioSourceNode which is
  // an AudioNode. AudioNode uses pre-finalizers to dispose the handler, so
  // holding a weak reference is ok here and will not interfer with garbage
  // collection.
  //
  // It is accessed by both audio and main thread. TODO: we really should
  // try to minimize or avoid the audio thread touching this element.
  CrossThreadWeakPersistent<HTMLMediaElement> media_element_;
  Mutex process_lock_;

  unsigned source_number_of_channels_;
  double source_sample_rate_;

  std::unique_ptr<MediaMultiChannelResampler> multi_channel_resampler_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // True if the orgin would be tainted by the media element.  In this case,
  // this node outputs silence.  This can happen if the media element source is
  // a cross-origin source which we're not allowed to access due to CORS
  // restrictions.
  bool is_origin_tainted_;
};

class MediaElementAudioSourceNode final : public AudioNode,
                                          public AudioSourceProviderClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaElementAudioSourceNode* Create(AudioContext&,
                                             HTMLMediaElement&,
                                             ExceptionState&);
  static MediaElementAudioSourceNode*
  Create(AudioContext*, const MediaElementAudioSourceOptions*, ExceptionState&);

  MediaElementAudioSourceNode(AudioContext&, HTMLMediaElement&);

  void Trace(Visitor*) const override;
  MediaElementAudioSourceHandler& GetMediaElementAudioSourceHandler() const;

  HTMLMediaElement* mediaElement() const;

  // AudioSourceProviderClient functions:
  void SetFormat(uint32_t number_of_channels, float sample_rate) override;
  void lock() override EXCLUSIVE_LOCK_FUNCTION(
      GetMediaElementAudioSourceHandler().GetProcessLock());
  void unlock() override
      UNLOCK_FUNCTION(GetMediaElementAudioSourceHandler().GetProcessLock());

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  Member<HTMLMediaElement> media_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_ELEMENT_AUDIO_SOURCE_NODE_H_
