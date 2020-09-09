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

#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_element_audio_source_options.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class MediaElementAudioSourceHandlerLocker final {
  STACK_ALLOCATED();

 public:
  MediaElementAudioSourceHandlerLocker(MediaElementAudioSourceHandler& lockable)
      : lockable_(lockable) {
    lockable_.lock();
  }
  ~MediaElementAudioSourceHandlerLocker() { lockable_.unlock(); }

 private:
  MediaElementAudioSourceHandler& lockable_;

  DISALLOW_COPY_AND_ASSIGN(MediaElementAudioSourceHandlerLocker);
};

MediaElementAudioSourceHandler::MediaElementAudioSourceHandler(
    AudioNode& node,
    HTMLMediaElement& media_element)
    : AudioHandler(kNodeTypeMediaElementAudioSource,
                   node,
                   node.context()->sampleRate()),
      media_element_(media_element),
      source_number_of_channels_(0),
      source_sample_rate_(0),
      is_origin_tainted_(false) {
  DCHECK(IsMainThread());
  // Default to stereo. This could change depending on what the media element
  // .src is set to.
  AddOutput(2);

  if (Context()->GetExecutionContext()) {
    task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMediaElementEvent);
  }

  Initialize();
}

scoped_refptr<MediaElementAudioSourceHandler>
MediaElementAudioSourceHandler::Create(AudioNode& node,
                                       HTMLMediaElement& media_element) {
  return base::AdoptRef(
      new MediaElementAudioSourceHandler(node, media_element));
}

MediaElementAudioSourceHandler::~MediaElementAudioSourceHandler() {
  Uninitialize();
}

CrossThreadPersistent<HTMLMediaElement>
MediaElementAudioSourceHandler::MediaElement() const {
  return media_element_.Lock();
}

void MediaElementAudioSourceHandler::Dispose() {
  AudioHandler::Dispose();
}

void MediaElementAudioSourceHandler::SetFormat(uint32_t number_of_channels,
                                               float source_sample_rate) {
  DCHECK(MediaElement());
  bool is_tainted = WouldTaintOrigin();

  if (is_tainted) {
    PrintCorsMessage(MediaElement()->currentSrc().GetString());
  }

  {
    // Make sure |is_origin_tainted_| matches |is_tainted|.  But need to
    // synchronize with process() to set this.
    MediaElementAudioSourceHandlerLocker locker(*this);
    is_origin_tainted_ = is_tainted;
  }

  if (number_of_channels != source_number_of_channels_ ||
      source_sample_rate != source_sample_rate_) {
    if (!number_of_channels ||
        number_of_channels > BaseAudioContext::MaxNumberOfChannels() ||
        !audio_utilities::IsValidAudioBufferSampleRate(source_sample_rate)) {
      // process() will generate silence for these uninitialized values.
      DLOG(ERROR) << "setFormat(" << number_of_channels << ", "
                  << source_sample_rate << ") - unhandled format change";
      // Synchronize with process().
      MediaElementAudioSourceHandlerLocker locker(*this);
      source_number_of_channels_ = 0;
      source_sample_rate_ = 0;
      return;
    }

    // Synchronize with process() to protect |source_number_of_channels_|,
    // |source_sample_rate_|, |multi_channel_resampler_|.
    MediaElementAudioSourceHandlerLocker locker(*this);

    source_number_of_channels_ = number_of_channels;
    source_sample_rate_ = source_sample_rate;

    if (source_sample_rate != Context()->sampleRate()) {
      double scale_factor = source_sample_rate / Context()->sampleRate();
      multi_channel_resampler_.reset(new MediaMultiChannelResampler(
          number_of_channels, scale_factor,
          audio_utilities::kRenderQuantumFrames,
          CrossThreadBindRepeating(
              &MediaElementAudioSourceHandler::ProvideResamplerInput,
              CrossThreadUnretained(this))));
    } else {
      // Bypass resampling.
      multi_channel_resampler_.reset();
    }

    {
      // The context must be locked when changing the number of output channels.
      BaseAudioContext::GraphAutoLocker context_locker(Context());

      // Do any necesssary re-configuration to the output's number of channels.
      Output(0).SetNumberOfChannels(number_of_channels);
    }
  }
}

bool MediaElementAudioSourceHandler::WouldTaintOrigin() {
  DCHECK(MediaElement());
  return MediaElement()->GetWebMediaPlayer()->WouldTaintOrigin();
}

void MediaElementAudioSourceHandler::PrintCorsMessage(const String& message) {
  if (Context()->GetExecutionContext()) {
    Context()->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kSecurity,
            mojom::ConsoleMessageLevel::kInfo,
            "MediaElementAudioSource outputs zeroes due to "
            "CORS access restrictions for " +
                message));
  }
}

void MediaElementAudioSourceHandler::ProvideResamplerInput(
    int resampler_frame_delay,
    AudioBus* dest) {
  DCHECK(Context()->IsAudioThread());
  DCHECK(MediaElement());
  DCHECK(dest);
  MediaElement()->GetAudioSourceProvider().ProvideInput(dest, dest->length());
}

void MediaElementAudioSourceHandler::Process(uint32_t number_of_frames) {
  AudioBus* output_bus = Output(0).Bus();

  // Use a tryLock() to avoid contention in the real-time audio thread.
  // If we fail to acquire the lock then the HTMLMediaElement must be in the
  // middle of reconfiguring its playback engine, so we output silence in this
  // case.
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked()) {
    if (!MediaElement() || !source_sample_rate_) {
      output_bus->Zero();
      return;
    }

    // TODO(crbug.com/811516): Although OnSetFormat() requested the output bus
    // channels, the actual channel count might have not been changed yet.
    // Output silence for such case until the channel count is resolved.
    if (source_number_of_channels_ != output_bus->NumberOfChannels()) {
      output_bus->Zero();
      return;
    }

    AudioSourceProvider& provider = MediaElement()->GetAudioSourceProvider();
    // Grab data from the provider so that the element continues to make
    // progress, even if we're going to output silence anyway.
    if (multi_channel_resampler_.get()) {
      DCHECK_NE(source_sample_rate_, Context()->sampleRate());
      multi_channel_resampler_->Resample(number_of_frames, output_bus);
    } else {
      // Bypass the resampler completely if the source is at the context's
      // sample-rate.
      DCHECK_EQ(source_sample_rate_, Context()->sampleRate());
      provider.ProvideInput(output_bus, number_of_frames);
    }
    // Output silence if we don't have access to the element.
    if (is_origin_tainted_) {
      output_bus->Zero();
    }
  } else {
    // We failed to acquire the lock.
    output_bus->Zero();
  }
}

void MediaElementAudioSourceHandler::lock() {
  process_lock_.lock();
}

void MediaElementAudioSourceHandler::unlock() {
  process_lock_.unlock();
}

// ----------------------------------------------------------------

MediaElementAudioSourceNode::MediaElementAudioSourceNode(
    AudioContext& context,
    HTMLMediaElement& media_element)
    : AudioNode(context), media_element_(&media_element) {
  SetHandler(MediaElementAudioSourceHandler::Create(*this, media_element));
}

MediaElementAudioSourceNode* MediaElementAudioSourceNode::Create(
    AudioContext& context,
    HTMLMediaElement& media_element,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // First check if this media element already has a source node.
  if (media_element.AudioSourceNode()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "HTMLMediaElement already connected "
                                      "previously to a different "
                                      "MediaElementSourceNode.");
    return nullptr;
  }

  MediaElementAudioSourceNode* node =
      MakeGarbageCollected<MediaElementAudioSourceNode>(context, media_element);

  if (node) {
    media_element.SetAudioSourceNode(node);
    // context keeps reference until node is disconnected
    context.NotifySourceNodeStartedProcessing(node);
    if (!context.HasRealtimeConstraint()) {
      Deprecation::CountDeprecation(
          node->GetExecutionContext(),
          WebFeature::kMediaElementSourceOnOfflineContext);
    }
  }

  return node;
}

MediaElementAudioSourceNode* MediaElementAudioSourceNode::Create(
    AudioContext* context,
    const MediaElementAudioSourceOptions* options,
    ExceptionState& exception_state) {
  return Create(*context, *options->mediaElement(), exception_state);
}

void MediaElementAudioSourceNode::Trace(Visitor* visitor) const {
  visitor->Trace(media_element_);
  AudioSourceProviderClient::Trace(visitor);
  AudioNode::Trace(visitor);
}

MediaElementAudioSourceHandler&
MediaElementAudioSourceNode::GetMediaElementAudioSourceHandler() const {
  return static_cast<MediaElementAudioSourceHandler&>(Handler());
}

HTMLMediaElement* MediaElementAudioSourceNode::mediaElement() const {
  return media_element_;
}

void MediaElementAudioSourceNode::SetFormat(uint32_t number_of_channels,
                                            float sample_rate) {
  GetMediaElementAudioSourceHandler().SetFormat(number_of_channels,
                                                sample_rate);
}

void MediaElementAudioSourceNode::lock() {
  GetMediaElementAudioSourceHandler().lock();
}

void MediaElementAudioSourceNode::unlock() {
  GetMediaElementAudioSourceHandler().unlock();
}

void MediaElementAudioSourceNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void MediaElementAudioSourceNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
