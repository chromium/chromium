/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/audio_destination.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/push_pull_fifo.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// This FIFO size of 16,384 was chosen based on the UMA data. It's the nearest
// multiple of 128 to 16,354 sample-frames, which represents 100% of the
// histogram from "WebAudio.AudioDestination.HardwareBufferSize".
// Although a buffer this big is atypical, some Android phones with a Bluetooth
// audio device report a large buffer size. This redundancy allows such device
// to play audio via Web Audio API.
constexpr uint32_t kFIFOSize = 128 * 128;

const char* DeviceStateToString(AudioDestination::DeviceState state) {
  switch (state) {
    case AudioDestination::kRunning:
      return "running";
    case AudioDestination::kPaused:
      return "paused";
    case AudioDestination::kStopped:
      return "stopped";
  }
}

}  // namespace

scoped_refptr<AudioDestination> AudioDestination::Create(
    AudioIOCallback& callback,
    const WebAudioSinkDescriptor& sink_descriptor,
    unsigned number_of_output_channels,
    const WebAudioLatencyHint& latency_hint,
    std::optional<float> context_sample_rate,
    unsigned render_quantum_frames) {
  TRACE_EVENT0("webaudio", "AudioDestination::Create");
  return base::AdoptRef(
      new AudioDestination(callback, sink_descriptor, number_of_output_channels,
                           latency_hint, context_sample_rate,
                           render_quantum_frames));
}

AudioDestination::~AudioDestination() {
  Stop();
}

int AudioDestination::Render(base::TimeDelta delay,
                             base::TimeTicks delay_timestamp,
                             const media::AudioGlitchInfo& glitch_info,
                             media::AudioBus* dest) {
  const uint32_t number_of_frames = dest->frames();

  TRACE_EVENT("webaudio", "AudioDestination::Render", "frames",
              number_of_frames, "playout_delay (ms)", delay.InMillisecondsF(),
              "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF());
  glitch_info.MaybeAddTraceEvent();

  CHECK_EQ(static_cast<size_t>(dest->channels()), number_of_output_channels_);
  CHECK_EQ(number_of_frames, callback_buffer_size_);

  if (!is_latency_metric_collected_ && delay.is_positive()) {
    // With the advanced distribution profile for a Bluetooth device
    // (potentially devices with the largest latency), the known latency is
    // around 100 ~ 150ms. Using a "linear" histogram where all buckets are
    // exactly the same size (2ms).
    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
        "WebAudio.AudioDestination.HardwareOutputLatency", 0, 200, 100,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(base::saturated_cast<int32_t>(delay.InMillisecondsF()));
    is_latency_metric_collected_ = true;
  }

  // Note that this method is called by AudioDeviceThread. If FIFO is not ready,
  // or the requested render size is greater than FIFO size return here.
  // (crbug.com/692423)
  if (!fifo_ || fifo_->length() < number_of_frames) {
    TRACE_EVENT_INSTANT1(
        "webaudio",
        "AudioDestination::Render - FIFO not ready or the size is too small",
        TRACE_EVENT_SCOPE_THREAD, "fifo length", fifo_ ? fifo_->length() : 0);
    return 0;
  }

  // Associate the destination data array with the output bus.
  for (unsigned i = 0; i < number_of_output_channels_; ++i) {
    output_bus_->SetChannelMemory(i, dest->channel(i), number_of_frames);
  }

  if (is_output_buffer_bypassed_) {
    // Fill the FIFO if necessary.
    const uint32_t frames_available = fifo_->GetFramesAvailable();
    const uint32_t frames_to_render = number_of_frames > frames_available
                                          ? number_of_frames - frames_available
                                          : 0;
    if (worklet_task_runner_) {
      // Use the dual-thread rendering if the AudioWorklet is activated.
      output_buffer_bypass_wait_event_.Reset();
      PostCrossThreadTask(
          *worklet_task_runner_, FROM_HERE,
          CrossThreadBindOnce(&AudioDestination::RequestRenderWait,
                              WrapRefCounted(this), number_of_frames,
                              frames_to_render, delay, delay_timestamp,
                              glitch_info));
      {
        TRACE_EVENT0("webaudio", "AudioDestination::Render waiting");
        base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
        // This is `Wait()`ing on the audio render thread for a `Signal()` from
        // the `worklet_task_runner_` thread, which will come from
        // `RequestRenderWait()`.
        //
        // `WaitableEvent` should generally not be allowed on the real-time
        // audio threads. In particular, no other code executed on the worklet
        // task runner thread should be using `WaitableEvent`. Additionally, the
        // below should be the only call to `Wait()` in `AudioDestination`.
        // Both the `Wait()` and `Signal()` should only be executed when the
        // kWebAudioBypassOutputBuffering flag is enabled, for testing output
        // latency differences when the output buffer is bypassed.
        //
        // As long as the above is true, it is not possible to deadlock or have
        // both threads waiting on each other. There is, however, no guarantee
        // that the task runner will finish within the real-time budget.
        output_buffer_bypass_wait_event_.Wait();
      }
    } else {
      // Otherwise use the single-thread rendering.
      RequestRender(number_of_frames, frames_to_render, delay, delay_timestamp,
                    glitch_info);
    }

    fifo_->Pull(output_bus_.get(), number_of_frames);

  } else {
    // Fill the FIFO.
    if (worklet_task_runner_) {
      // Use the dual-thread rendering if the AudioWorklet is activated.
      auto result =
          fifo_->PullAndUpdateEarmark(output_bus_.get(), number_of_frames);
      // The audio that we just pulled from the fifo will be played before the
      // audio that we are about to request, so we add that duration to the
      // delay of the audio we request. Note that it doesn't matter if there was
      // a fifo underrun, the delay will be the same either way.
      delay += audio_utilities::FramesToTime(number_of_frames,
                                             web_audio_device_->SampleRate());

      media::AudioGlitchInfo combined_glitch_info = glitch_info;
      if (result.frames_provided < number_of_frames) {
        media::AudioGlitchInfo underrun{
            // FIFO contains audio at the output device sample rate.
            .duration = audio_utilities::FramesToTime(
                number_of_frames - result.frames_provided,
                web_audio_device_->SampleRate()),
            .count = 1};
        underrun.MaybeAddTraceEvent();
        combined_glitch_info += underrun;
      }

      PostCrossThreadTask(
          *worklet_task_runner_, FROM_HERE,
          CrossThreadBindOnce(&AudioDestination::RequestRender,
                              WrapRefCounted(this), number_of_frames,
                              result.frames_to_render, delay, delay_timestamp,
                              combined_glitch_info));
    } else {
      // Otherwise use the single-thread rendering.
      const size_t frames_to_render =
          fifo_->Pull(output_bus_.get(), number_of_frames);
      // The audio that we just pulled from the fifo will be played before the
      // audio that we are about to request, so we add that duration to the
      // delay of the audio we request.
      delay += audio_utilities::FramesToTime(number_of_frames,
                                             web_audio_device_->SampleRate());
      RequestRender(number_of_frames, frames_to_render, delay, delay_timestamp,
                    glitch_info);
    }
  }

  return number_of_frames;
}

void AudioDestination::OnRenderError() {
  DCHECK(IsMainThread());

  callback_->OnRenderError();
}

void AudioDestination::Start() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::Start");
  SendLogMessage(__func__, "");

  if (device_state_ != DeviceState::kStopped) {
    return;
  }
  web_audio_device_->Start();
  SetDeviceState(DeviceState::kRunning);
}

void AudioDestination::Stop() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::Stop");
  SendLogMessage(__func__, "");

  if (device_state_ == DeviceState::kStopped) {
    return;
  }
  web_audio_device_->Stop();

  // Resetting `worklet_task_runner_` here is safe because
  // AudioDestination::Render() won't be called after WebAudioDevice::Stop()
  // call above.
  worklet_task_runner_ = nullptr;

  SetDeviceState(DeviceState::kStopped);
}

void AudioDestination::Pause() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::Pause");
  SendLogMessage(__func__, "");

  if (device_state_ != DeviceState::kRunning) {
    return;
  }
  web_audio_device_->Pause();
  SetDeviceState(DeviceState::kPaused);
}

void AudioDestination::Resume() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::Resume");
  SendLogMessage(__func__, "");

  if (device_state_ != DeviceState::kPaused) {
    return;
  }
  web_audio_device_->Resume();
  SetDeviceState(DeviceState::kRunning);
}

void AudioDestination::SetWorkletTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> worklet_task_runner) {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::SetWorkletTaskRunner");

  if (worklet_task_runner_) {
    DCHECK_EQ(worklet_task_runner_, worklet_task_runner);
    return;
  }

  // The dual-thread rendering kicks off, so update the earmark frames
  // accordingly.
  fifo_->SetEarmarkFrames(callback_buffer_size_);
  worklet_task_runner_ = std::move(worklet_task_runner);
}

void AudioDestination::StartWithWorkletTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> worklet_task_runner) {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::StartWithWorkletTaskRunner");
  SendLogMessage(__func__, "");

  if (device_state_ != DeviceState::kStopped) {
    return;
  }

  SetWorkletTaskRunner(worklet_task_runner);
  web_audio_device_->Start();
  SetDeviceState(DeviceState::kRunning);
}

bool AudioDestination::IsPlaying() {
  DCHECK(IsMainThread());
  return device_state_ == DeviceState::kRunning;
}

double AudioDestination::SampleRate() const {
  return context_sample_rate_;
}

uint32_t AudioDestination::CallbackBufferSize() const {
  return callback_buffer_size_;
}

int AudioDestination::FramesPerBuffer() const {
  DCHECK(IsMainThread());
  return web_audio_device_->FramesPerBuffer();
}

base::TimeDelta AudioDestination::GetPlatformBufferDuration() const {
  DCHECK(IsMainThread());
  return audio_utilities::FramesToTime(web_audio_device_->FramesPerBuffer(),
                                       web_audio_device_->SampleRate());
}

uint32_t AudioDestination::MaxChannelCount() const {
  return web_audio_device_->MaxChannelCount();
}

void AudioDestination::SetDetectSilence(bool detect_silence) {
  DCHECK(IsMainThread());
  TRACE_EVENT1("webaudio", "AudioDestination::SetDetectSilence",
               "detect_silence", detect_silence);
  SendLogMessage(__func__,
                 String::Format("({detect_silence=%d})", detect_silence));

  web_audio_device_->SetDetectSilence(detect_silence);
}

AudioDestination::AudioDestination(
    AudioIOCallback& callback,
    const WebAudioSinkDescriptor& sink_descriptor,
    unsigned number_of_output_channels,
    const WebAudioLatencyHint& latency_hint,
    std::optional<float> context_sample_rate,
    unsigned render_quantum_frames)
    : web_audio_device_(
          Platform::Current()->CreateAudioDevice(sink_descriptor,
                                                 number_of_output_channels,
                                                 latency_hint,
                                                 this)),
      callback_buffer_size_(
          web_audio_device_ ? web_audio_device_->FramesPerBuffer() : 0),
      number_of_output_channels_(number_of_output_channels),
      render_quantum_frames_(render_quantum_frames),
      context_sample_rate_(
          context_sample_rate.has_value()
              ? context_sample_rate.value()
              : (web_audio_device_ ? web_audio_device_->SampleRate() : 0)),
      fifo_(std::make_unique<PushPullFIFO>(
          number_of_output_channels,
          std::max(kFIFOSize, callback_buffer_size_ + render_quantum_frames),
          render_quantum_frames)),
      output_bus_(AudioBus::Create(number_of_output_channels,
                                   render_quantum_frames,
                                   false)),
      render_bus_(
          AudioBus::Create(number_of_output_channels, render_quantum_frames)),
      callback_(callback),
      is_output_buffer_bypassed_(base::FeatureList::IsEnabled(
          features::kWebAudioBypassOutputBuffering)) {
  CHECK(web_audio_device_);

  SendLogMessage(__func__, String::Format("({output_channels=%u})",
                                          number_of_output_channels));
  SendLogMessage(__func__,
                 String::Format("=> (FIFO size=%u bytes)", fifo_->length()));

  SendLogMessage(__func__,
                 String::Format("=> (device callback buffer size=%u frames)",
                                callback_buffer_size_));
  SendLogMessage(__func__, String::Format("=> (device sample rate=%.0f Hz)",
                                          web_audio_device_->SampleRate()));

  TRACE_EVENT1("webaudio", "AudioDestination::AudioDestination",
               "sink information",
               audio_utilities::GetSinkInfoForTracing(
                   sink_descriptor, latency_hint,
                   number_of_output_channels, web_audio_device_->SampleRate(),
                   callback_buffer_size_));

  metric_reporter_.Initialize(
      callback_buffer_size_, web_audio_device_->SampleRate());

  if (!is_output_buffer_bypassed_) {
    // Primes the FIFO for the given callback buffer size. This is to prevent
    // first FIFO pulls from causing "underflow" errors.
    const unsigned priming_render_quanta =
        ceil(callback_buffer_size_ / static_cast<float>(render_quantum_frames));
    for (unsigned i = 0; i < priming_render_quanta; ++i) {
      fifo_->Push(render_bus_.get());
    }
  }

  double scale_factor = 1.0;

  if (context_sample_rate_ != web_audio_device_->SampleRate()) {
    scale_factor = context_sample_rate_ / web_audio_device_->SampleRate();
    SendLogMessage(__func__,
                   String::Format("=> (resampling from %0.f Hz to %0.f Hz)",
                                  context_sample_rate.value(),
                                  web_audio_device_->SampleRate()));

    resampler_ = std::make_unique<MediaMultiChannelResampler>(
        number_of_output_channels, scale_factor, render_quantum_frames,
        CrossThreadBindRepeating(&AudioDestination::ProvideResamplerInput,
                                 CrossThreadUnretained(this)));
    resampler_bus_ =
        media::AudioBus::CreateWrapper(render_bus_->NumberOfChannels());
    for (unsigned int i = 0; i < render_bus_->NumberOfChannels(); ++i) {
      resampler_bus_->SetChannelData(i, render_bus_->Channel(i)->MutableData());
    }
    resampler_bus_->set_frames(render_bus_->length());
  } else {
    SendLogMessage(
        __func__,
        String::Format("=> (no resampling: context sample rate set to %0.f Hz)",
                       context_sample_rate_));
  }

  // Record the sizes if we successfully created an output device.
  // Histogram for audioHardwareBufferSize
  base::UmaHistogramSparse(
      "WebAudio.AudioDestination.HardwareBufferSize",
      static_cast<int>(Platform::Current()->AudioHardwareBufferSize()));

  // Histogram for the actual callback size used.  Typically, this is the same
  // as audioHardwareBufferSize, but can be adjusted depending on some
  // heuristics below.
  base::UmaHistogramSparse("WebAudio.AudioDestination.CallbackBufferSize",
                           callback_buffer_size_);

  base::UmaHistogramSparse("WebAudio.AudioContext.HardwareSampleRate",
                           web_audio_device_->SampleRate());

  // Record the selected sample rate and ratio if the sampleRate was given.  The
  // ratio is recorded as a percentage, rounded to the nearest percent.
  if (context_sample_rate.has_value()) {
    // The actual supplied `context_sample_rate` is probably a small set
    // including 44100, 48000, 22050, and 2400 Hz.  Other valid values range
    // from 3000 to 384000 Hz, but are not expected to be used much.
    base::UmaHistogramSparse("WebAudio.AudioContextOptions.sampleRate",
                             context_sample_rate.value());
    // From the expected values above and the common HW sample rates, we expect
    // the most common ratios to be the set 0.5, 44100/48000, and 48000/44100.
    // Other values are possible but seem unlikely.
    base::UmaHistogramSparse("WebAudio.AudioContextOptions.sampleRateRatio",
                             static_cast<int32_t>(100.0 * scale_factor + 0.5));
  }
}

void AudioDestination::SetDeviceState(DeviceState state) {
  DCHECK(IsMainThread());
  base::AutoLock locker(device_state_lock_);

  device_state_ = state;
}

void AudioDestination::RequestRenderWait(
    size_t frames_requested,
    size_t frames_to_render,
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    const media::AudioGlitchInfo& glitch_info) {
  RequestRender(frames_requested, frames_to_render, delay, delay_timestamp,
                glitch_info);
  output_buffer_bypass_wait_event_.Signal();
}

void AudioDestination::RequestRender(
    size_t frames_requested,
    size_t frames_to_render,
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    const media::AudioGlitchInfo& glitch_info) {

  base::AutoTryLock locker(device_state_lock_);

  TRACE_EVENT("webaudio", "AudioDestination::RequestRender", "frames_requested",
              frames_requested, "frames_to_render", frames_to_render,
              "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF(),
              "playout_delay (ms)", delay.InMillisecondsF(), "delay (frames)",
              fifo_->GetFramesAvailable());

  // The state might be changing by ::Stop() call. If the state is locked, do
  // not touch the below.
  if (!locker.is_acquired()) {
    return;
  }

  if (device_state_ != DeviceState::kRunning) {
    return;
  }

  metric_reporter_.BeginTrace();

  if (frames_elapsed_ == 0) {
    SendLogMessage(__func__, String::Format("=> (rendering is now alive)"));
  }

  // FIFO contains audio at the output device sample rate.
  delay_to_report_ =
      delay + audio_utilities::FramesToTime(fifo_->GetFramesAvailable(),
                                            web_audio_device_->SampleRate());

  glitch_info_to_report_.Add(glitch_info);

  output_position_.position =
      frames_elapsed_ / static_cast<double>(web_audio_device_->SampleRate()) -
      delay.InSecondsF();
  output_position_.timestamp =
      (delay_timestamp - base::TimeTicks()).InSecondsF();
  output_position_.hardware_output_latency = delay.InSecondsF();
  const base::TimeTicks callback_request = base::TimeTicks::Now();

  for (size_t pushed_frames = 0; pushed_frames < frames_to_render;
       pushed_frames += render_quantum_frames_) {
    // If platform buffer is more than two times longer than
    // `RenderQuantumFrames` we do not want output position to get stuck so we
    // promote it using the elapsed time from the moment it was initially
    // obtained.
    if (callback_buffer_size_ > render_quantum_frames_ * 2) {
      const double delta =
          (base::TimeTicks::Now() - callback_request).InSecondsF();
      output_position_.position += delta;
      output_position_.timestamp += delta;
    }

    // Some implementations give only rough estimation of `delay` so
    // we might have negative estimation `output_position_` value.
    if (output_position_.position < 0.0) {
      output_position_.position = 0.0;
    }

    // Process WebAudio graph and push the rendered output to FIFO.
    if (resampler_) {
      resampler_->ResampleInternal(render_quantum_frames_,
                                   resampler_bus_.get());
    } else {
      // Process WebAudio graph and push the rendered output to FIFO.
      PullFromCallback(render_bus_.get(), delay_to_report_);
    }

    fifo_->Push(render_bus_.get());
  }

  frames_elapsed_ += frames_requested;

  metric_reporter_.EndTrace();
}

void AudioDestination::ProvideResamplerInput(int resampler_frame_delay,
                                             AudioBus* dest) {
  // Resampler delay is audio frames at the context sample rate, before
  // resampling.
  TRACE_EVENT("webaudio", "AudioDestination::ProvideResamplerInput",
              "delay (frames)", resampler_frame_delay);
  auto adjusted_delay =
      delay_to_report_ + audio_utilities::FramesToTime(resampler_frame_delay,
                                                       context_sample_rate_);
  PullFromCallback(dest, adjusted_delay);
}

void AudioDestination::PullFromCallback(AudioBus* destination_bus,
                                        base::TimeDelta delay) {
  callback_->Render(destination_bus, render_quantum_frames_, output_position_,
                    metric_reporter_.GetMetric(), delay,
                    glitch_info_to_report_.GetAndReset());
}

media::OutputDeviceStatus AudioDestination::MaybeCreateSinkAndGetStatus() {
  TRACE_EVENT0("webaudio", "AudioDestination::MaybeCreateSinkAndGetStatus");
  return web_audio_device_->MaybeCreateSinkAndGetStatus();
}

void AudioDestination::SendLogMessage(const char* const function_name,
                                      const String& message) const {
  WebRtcLogMessage(String::Format("[WA]AD::%s %s [state=%s]", function_name,
                                  message.Utf8().c_str(),
                                  DeviceStateToString(device_state_))
                       .Utf8());
}

}  // namespace blink
