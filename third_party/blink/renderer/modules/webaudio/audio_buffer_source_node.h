/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BUFFER_SOURCE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BUFFER_SOURCE_NODE_H_

#include <atomic>
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/audio_scheduled_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/panner_node.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class AudioBufferSourceOptions;
class BaseAudioContext;

// AudioBufferSourceNode is an AudioNode representing an audio source from an
// in-memory audio asset represented by an AudioBuffer.  It generally will be
// used for short sounds which require a high degree of scheduling flexibility
// (can playback in rhythmically perfect ways).

class AudioBufferSourceHandler final : public AudioScheduledSourceHandler {
 public:
  static scoped_refptr<AudioBufferSourceHandler> Create(
      AudioNode&,
      float sample_rate,
      AudioParamHandler& playback_rate,
      AudioParamHandler& detune);
  ~AudioBufferSourceHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;

  // setBuffer() is called on the main thread. This is the buffer we use for
  // playback.
  void SetBuffer(AudioBuffer*, ExceptionState&);
  SharedAudioBuffer* Buffer() { return shared_buffer_.get(); }

  // numberOfChannels() returns the number of output channels.  This value
  // equals the number of channels from the buffer.  If a new buffer is set with
  // a different number of channels, then this value will dynamically change.
  unsigned NumberOfChannels();

  // Play-state
  void Start(double when, ExceptionState&);
  void Start(double when, double grain_offset, ExceptionState&);
  void Start(double when,
             double grain_offset,
             double grain_duration,
             ExceptionState&);

  // Note: the attribute was originally exposed as |.looping|, but to be more
  // consistent in naming with <audio> and with how it's described in the
  // specification, the proper attribute name is |.loop|. The old attribute is
  // kept for backwards compatibility.
  bool Loop() const { return is_looping_; }
  void SetLoop(bool looping) {
    is_looping_ = looping;
    SetDidSetLooping(looping);
  }

  // Loop times in seconds.
  double LoopStart() const { return loop_start_; }
  double LoopEnd() const { return loop_end_; }
  void SetLoopStart(double loop_start) { loop_start_ = loop_start; }
  void SetLoopEnd(double loop_end) { loop_end_ = loop_end; }

  // If we are no longer playing, propogate silence ahead to downstream nodes.
  bool PropagatesSilence() const override;

  void HandleStoppableSourceNode() override;

 private:
  AudioBufferSourceHandler(AudioNode&,
                           float sample_rate,
                           AudioParamHandler& playback_rate,
                           AudioParamHandler& detune);
  void StartSource(double when,
                   double grain_offset,
                   double grain_duration,
                   bool is_duration_given,
                   ExceptionState&);

  // Render audio directly from the buffer to the audio bus. Returns true on
  // success, i.e., audio was written to the output bus because all the internal
  // checks passed.
  //
  //   output_bus -
  //     AudioBus where the rendered audio goes.
  //   destination_frame_offset -
  //     Index into the output bus where the first frame should be written.
  //   number_of_frames -
  //     Maximum number of frames to process; this can be less that a render
  //     quantum.
  //   start_time_offset -
  //     Actual start time relative to the |destination_frame_offset|.  This
  //     should be the \sart_time_offset| value returned by
  //     |UpdateSchedulingInfo|.
  bool RenderFromBuffer(AudioBus* output_bus,
                        unsigned destination_frame_offset,
                        uint32_t number_of_frames,
                        double start_time_offset);

  // Render silence starting from "index" frame in AudioBus.
  inline bool RenderSilenceAndFinishIfNotLooping(AudioBus*,
                                                 unsigned index,
                                                 uint32_t frames_to_process);

  // Clamps grain parameters to the duration of the given AudioBuffer.
  void ClampGrainParameters(const SharedAudioBuffer*);

  // Sample data for the outputs of this node. The shared buffer can safely be
  // accessed from the audio thread.
  std::unique_ptr<SharedAudioBuffer> shared_buffer_;

  // Pointers for the buffer and destination.
  std::unique_ptr<const float* []> source_channels_;
  std::unique_ptr<float* []> destination_channels_;

  scoped_refptr<AudioParamHandler> playback_rate_;
  scoped_refptr<AudioParamHandler> detune_;

  bool DidSetLooping() const {
    return did_set_looping_.load(std::memory_order_acquire);
  }
  void SetDidSetLooping(bool loop) {
    if (loop)
      did_set_looping_.store(true, std::memory_order_release);
  }

  // If m_isLooping is false, then this node will be done playing and become
  // inactive after it reaches the end of the sample data in the buffer.  If
  // true, it will wrap around to the start of the buffer each time it reaches
  // the end.
  bool is_looping_;

  // True if the source .loop attribute was ever set.
  std::atomic_bool did_set_looping_;

  double loop_start_;
  double loop_end_;

  // m_virtualReadIndex is a sample-frame index into our buffer representing the
  // current playback position.  Since it's floating-point, it has sub-sample
  // accuracy.
  double virtual_read_index_;

  // Granular playback
  bool is_grain_;
  double grain_offset_;    // in seconds
  double grain_duration_;  // in seconds
  // True if grainDuration is given explicitly (via 3 arg start method).
  bool is_duration_given_;

  // Compute playback rate (k-rate) by incorporating the sample rate
  // conversion factor, and the value of playbackRate and detune AudioParams.
  double ComputePlaybackRate();

  double GetMinPlaybackRate();

  // The minimum playbackRate value ever used for this source.
  double min_playback_rate_;

  // True if the |buffer| attribute has ever been set to a non-null
  // value.  Defaults to false.
  bool buffer_has_been_set_;
};

class AudioBufferSourceNode final : public AudioScheduledSourceNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioBufferSourceNode* Create(BaseAudioContext&, ExceptionState&);
  static AudioBufferSourceNode* Create(BaseAudioContext*,
                                       AudioBufferSourceOptions*,
                                       ExceptionState&);
  AudioBufferSourceNode(BaseAudioContext&);
  void Trace(blink::Visitor*) override;
  AudioBufferSourceHandler& GetAudioBufferSourceHandler() const;

  AudioBuffer* buffer() const;
  void setBuffer(AudioBuffer*, ExceptionState&);
  AudioParam* playbackRate() const;
  AudioParam* detune() const;
  bool loop() const;
  void setLoop(bool);
  double loopStart() const;
  void setLoopStart(double);
  double loopEnd() const;
  void setLoopEnd(double);

  void start(ExceptionState&);
  void start(double when, ExceptionState&);
  void start(double when, double grain_offset, ExceptionState&);
  void start(double when,
             double grain_offset,
             double grain_duration,
             ExceptionState&);

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  Member<AudioParam> playback_rate_;
  Member<AudioParam> detune_;
  Member<AudioBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BUFFER_SOURCE_NODE_H_
