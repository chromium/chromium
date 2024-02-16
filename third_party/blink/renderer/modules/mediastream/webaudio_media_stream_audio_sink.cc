// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/webaudio_media_stream_audio_sink.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/platform/media/web_audio_source_provider_client.h"

namespace blink {

// Size of the buffer that WebAudio processes each time, it is the same value
// as AudioNode::ProcessingSizeInFrames in WebKit.
// static
const int WebAudioMediaStreamAudioSink::kWebAudioRenderBufferSize = 128;

WebAudioMediaStreamAudioSink::WebAudioMediaStreamAudioSink(
    MediaStreamComponent* component,
    int context_sample_rate,
    uint32_t context_buffer_size)
    : is_enabled_(false),
      component_(component),
      track_stopped_(false),
      sink_context_buffer_size_(context_buffer_size),
      sink_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                   media::ChannelLayoutConfig::Stereo(),
                   context_sample_rate,
                   kWebAudioRenderBufferSize) {
  CHECK(sink_params_.IsValid());
  CHECK(sink_context_buffer_size_);

  // Connect the source provider to the track as a sink.
  WebMediaStreamAudioSink::AddToAudioTrack(
      this, WebMediaStreamTrack(component_.Get()));
}

WebAudioMediaStreamAudioSink::~WebAudioMediaStreamAudioSink() {
  if (audio_converter_.get())
    audio_converter_->RemoveInput(this);

  // If the track is still active, it is necessary to notify the track before
  // the source provider goes away.
  if (!track_stopped_) {
    WebMediaStreamAudioSink::RemoveFromAudioTrack(
        this, WebMediaStreamTrack(component_.Get()));
  }
}

void WebAudioMediaStreamAudioSink::OnSetFormat(
    const media::AudioParameters& params) {
  CHECK(params.IsValid());

  base::AutoLock auto_lock(lock_);

  source_params_ = params;
  // Create the audio converter with |disable_fifo| as false so that the
  // converter will request source_params.frames_per_buffer() each time.
  // This will not increase the complexity as there is only one client to
  // the converter.
  audio_converter_ = std::make_unique<media::AudioConverter>(
      source_params_, sink_params_, false);
  audio_converter_->AddInput(this);

  // `fifo_` receives audio in OnData() in buffers of a size defined by
  // `source_params_`. It is consumed by `audio_converter_`  in buffers of the
  // same size. `audio_converter_` resamples from source_params_.sample_rate()
  // to sink_params_.sample_rate() and rebuffers into kWebAudioRenderBufferSize
  // chunks. However `audio_converter_->Convert()` are not spaced evenly: they
  // will come in batches as the audio context is filling up the output buffer
  // of `sink_context_buffer_size_' while rendering the media stream via
  // AudioContext.

  // To ensure ChunkSize() is correct: see AudioConverter documentation.
  audio_converter_->PrimeWithSilence();
  const int chunk_size = audio_converter_->ChunkSize();
  CHECK_GT(chunk_size, 0);
  const int max_batch_read_count =
      ceil(static_cast<double>(sink_context_buffer_size_) / chunk_size);

  // Due to resampling/rebuffering, audio consumption irregularities, and
  // possible misalignments of audio production/consumption callbacks, we should
  // be able to store audio for multiple batch-pulls.
  const size_t kMaxNumberOfBatchReads = 5;
  fifo_ = std::make_unique<media::AudioFifo>(
      source_params_.channels(), kMaxNumberOfBatchReads * max_batch_read_count *
                                     source_params_.frames_per_buffer());

  DVLOG(1) << "FIFO size: " << fifo_->max_frames()
           << " source buffer size: " << source_params_.frames_per_buffer()
           << " sink context buffer size: " << sink_context_buffer_size_
           << " chunk size " << chunk_size
           << " max batch read count: " << max_batch_read_count
           << " FIFO duration: "
           << fifo_->max_frames() * 1000 / source_params_.sample_rate()
           << " ms ";
}

void WebAudioMediaStreamAudioSink::OnReadyStateChanged(
    WebMediaStreamSource::ReadyState state) {
  NON_REENTRANT_SCOPE(ready_state_reentrancy_checker_);
  if (state == WebMediaStreamSource::kReadyStateEnded)
    track_stopped_ = true;
}

void WebAudioMediaStreamAudioSink::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  NON_REENTRANT_SCOPE(capture_reentrancy_checker_);
  DCHECK(!estimated_capture_time.is_null());
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::OnData", "this",
               static_cast<void*>(this), "frames", audio_bus.frames());

  base::AutoLock auto_lock(lock_);
  if (!is_enabled_)
    return;

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::OnData under lock");

  CHECK(fifo_.get());
  CHECK_EQ(audio_bus.channels(), source_params_.channels());
  CHECK_EQ(audio_bus.frames(), source_params_.frames_per_buffer());

  if (fifo_->frames() + audio_bus.frames() <= fifo_->max_frames()) {
    fifo_->Push(&audio_bus);
    TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("mediastream"),
                      "WebAudioMediaStreamAudioSink fifo space", this,
                      fifo_->max_frames() - fifo_->frames());
  } else {
    // This can happen if the data in FIFO is too slowly consumed or
    // WebAudio stops consuming data.

    DVLOG(2) << "WARNING: Overrun, FIFO has available "
             << (fifo_->max_frames() - fifo_->frames()) << " samples but "
             << audio_bus.frames() << " samples are needed";
    if (fifo_stats_) {
      fifo_stats_->overruns++;
    }

    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
                 "WebAudioMediaStreamAudioSink::OnData FIFO full");
  }
}

void WebAudioMediaStreamAudioSink::SetClient(
    WebAudioSourceProviderClient* client) {
  NOTREACHED();
}

void WebAudioMediaStreamAudioSink::ProvideInput(
    const WebVector<float*>& audio_data,
    int number_of_frames) {
  NON_REENTRANT_SCOPE(provide_input_reentrancy_checker_);
  DCHECK_EQ(number_of_frames, kWebAudioRenderBufferSize);

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::ProvideInput", "this",
               static_cast<void*>(this), "frames", number_of_frames);

  if (!output_wrapper_ ||
      static_cast<size_t>(output_wrapper_->channels()) != audio_data.size()) {
    output_wrapper_ =
        media::AudioBus::CreateWrapper(static_cast<int>(audio_data.size()));
  }

  output_wrapper_->set_frames(number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    output_wrapper_->SetChannelData(static_cast<int>(i), audio_data[i]);

  base::AutoLock auto_lock(lock_);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::ProvideInput under lock");

  if (!audio_converter_)
    return;

  is_enabled_ = true;
  audio_converter_->Convert(output_wrapper_.get());
}

void WebAudioMediaStreamAudioSink::ResetFifoStatsForTesting() {
  fifo_stats_ = std::make_unique<FifoStats>();
}

const WebAudioMediaStreamAudioSink::FifoStats&
WebAudioMediaStreamAudioSink::GetFifoStatsForTesting() {
  CHECK(fifo_stats_) << "Call ResetFifoStatsForTesting() to enable";
  return *fifo_stats_;
}

// |lock_| needs to be acquired before this function is called. It's called by
// AudioConverter which in turn is called by the above ProvideInput() function.
// Thus thread safety analysis is disabled here and |lock_| acquire manually
// asserted.
double WebAudioMediaStreamAudioSink::ProvideInput(
    media::AudioBus* audio_bus,
    uint32_t frames_delayed,
    const media::AudioGlitchInfo& glitch_info) NO_THREAD_SAFETY_ANALYSIS {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::ProvideInput 2");

  lock_.AssertAcquired();
  CHECK(fifo_);
  if (fifo_->frames() >= audio_bus->frames()) {
    fifo_->Consume(audio_bus, 0, audio_bus->frames());
    TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("mediastream"),
                      "WebAudioMediaStreamAudioSink fifo space", this,
                      fifo_->max_frames() - fifo_->frames());
  } else {
    DVLOG(2) << "WARNING: Underrun, FIFO has data " << fifo_->frames()
             << " samples but " << audio_bus->frames() << " samples are needed";
    audio_bus->Zero();
    if (fifo_stats_) {
      fifo_stats_->underruns++;
    }
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("mediastream"),
                 "WebAudioMediaStreamAudioSink::ProvideInput underrun",
                 "frames missing", audio_bus->frames() - fifo_->frames());
  }

  return 1.0;
}


}  // namespace blink
