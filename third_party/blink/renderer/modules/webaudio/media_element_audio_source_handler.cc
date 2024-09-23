// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_handler.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_element_audio_source_options.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// Default to stereo. This could change depending on what the media element
// .src is set to.
constexpr unsigned kDefaultNumberOfOutputChannels = 2;

}  // namespace

class MediaElementAudioSourceHandlerLocker final {
  STACK_ALLOCATED();

 public:
  explicit MediaElementAudioSourceHandlerLocker(
      MediaElementAudioSourceHandler& lockable)
      : lockable_(lockable) {
    lockable_.lock();
  }

  MediaElementAudioSourceHandlerLocker(
      const MediaElementAudioSourceHandlerLocker&) = delete;
  MediaElementAudioSourceHandlerLocker& operator=(
      const MediaElementAudioSourceHandlerLocker&) = delete;

  ~MediaElementAudioSourceHandlerLocker() { lockable_.unlock(); }

 private:
  MediaElementAudioSourceHandler& lockable_;
};

MediaElementAudioSourceHandler::MediaElementAudioSourceHandler(
    AudioNode& node,
    HTMLMediaElement& media_element)
    : AudioHandler(kNodeTypeMediaElementAudioSource,
                   node,
                   node.context()->sampleRate()),
      media_element_(media_element) {
  DCHECK(IsMainThread());

  AddOutput(kDefaultNumberOfOutputChannels);

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
    // Make sure `is_origin_tainted_` matches `is_tainted`.  But need to
    // synchronize with `Process()` to set this.
    MediaElementAudioSourceHandlerLocker locker(*this);
    is_origin_tainted_ = is_tainted;
  }

  if (number_of_channels != source_number_of_channels_ ||
      source_sample_rate != source_sample_rate_) {
    if (!number_of_channels ||
        number_of_channels > BaseAudioContext::MaxNumberOfChannels() ||
        !audio_utilities::IsValidAudioBufferSampleRate(source_sample_rate)) {
      // `Process()` will generate silence for these uninitialized values.
      DLOG(ERROR) << "setFormat(" << number_of_channels << ", "
                  << source_sample_rate << ") - unhandled format change";
      // Synchronize with `Process()`.
      MediaElementAudioSourceHandlerLocker locker(*this);
      source_number_of_channels_ = 0;
      source_sample_rate_ = 0;
      return;
    }

    // Synchronize with `Process()` to protect `source_number_of_channels_`,
    // `source_sample_rate_`, `multi_channel_resampler_`.
    MediaElementAudioSourceHandlerLocker locker(*this);

    source_number_of_channels_ = number_of_channels;
    source_sample_rate_ = source_sample_rate;

    if (source_sample_rate != Context()->sampleRate()) {
      double scale_factor = source_sample_rate / Context()->sampleRate();
      multi_channel_resampler_ = std::make_unique<MediaMultiChannelResampler>(
          number_of_channels, scale_factor,
          GetDeferredTaskHandler().RenderQuantumFrames(),
          CrossThreadBindRepeating(
              &MediaElementAudioSourceHandler::ProvideResamplerInput,
              CrossThreadUnretained(this)));
    } else {
      // Bypass resampling.
      multi_channel_resampler_.reset();
    }

    {
      // The context must be locked when changing the number of output channels.
      DeferredTaskHandler::GraphAutoLocker context_locker(Context());

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
  MediaElement()->GetAudioSourceProvider().ProvideInput(
      dest, base::checked_cast<int>(dest->length()));
}

void MediaElementAudioSourceHandler::Process(uint32_t number_of_frames) {
  AudioBus* output_bus = Output(0).Bus();

  // Use a tryLock() to avoid contention in the real-time audio thread.
  // If we fail to acquire the lock then the HTMLMediaElement must be in the
  // middle of reconfiguring its playback engine, so we output silence in this
  // case.
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
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
    const int frames_int = base::checked_cast<int>(number_of_frames);
    if (multi_channel_resampler_.get()) {
      DCHECK_NE(source_sample_rate_, Context()->sampleRate());
      multi_channel_resampler_->Resample(frames_int, output_bus);
    } else {
      // Bypass the resampler completely if the source is at the context's
      // sample-rate.
      DCHECK_EQ(source_sample_rate_, Context()->sampleRate());
      provider.ProvideInput(output_bus, frames_int);
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
  process_lock_.Acquire();
}

void MediaElementAudioSourceHandler::unlock() {
  process_lock_.Release();
}

}  // namespace blink
