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
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
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
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// FIFO Size.
//
// TODO(hongchan): This was estimated based on the largest callback buffer size
// that we would ever need. The current UMA stats indicates that this is, in
// fact, probably too small. There are Android devices out there with a size of
// 8000 or so.  We might need to make this larger. See: crbug.com/670747
constexpr uint32_t kFIFOSize = 96 * 128;

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
    absl::optional<float> context_sample_rate,
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
  const double delay_seconds = delay.InSecondsF();
  const double delay_timestamp_seconds =
      (delay_timestamp - base::TimeTicks()).InSecondsF();

  TRACE_EVENT_BEGIN1("webaudio", "AudioDestination::Render",
                     "callback_buffer_size", number_of_frames);
  CHECK_EQ(static_cast<size_t>(dest->channels()), number_of_output_channels_);
  CHECK_EQ(number_of_frames, callback_buffer_size_);

  if (!is_latency_metric_collected_ && delay_seconds != 0.0) {
    // With the advanced distribution profile for a Bluetooth device
    // (potentially devices with the largest latency), the known latency is
    // around 100 ~ 150ms. Using a "linear" histogram where all buckets are
    // exactly the same size (2ms).
    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
        "WebAudio.AudioDestination.HardwareOutputLatency", 0, 200, 100,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(static_cast<int32_t>(delay_seconds * 1000));
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
    TRACE_EVENT_END2("webaudio", "AudioDestination::Render", "timestamp (s)",
                     delay_timestamp_seconds, "delay (s)", delay_seconds);
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
                              frames_to_render, delay_seconds,
                              delay_timestamp_seconds));
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
      RequestRender(number_of_frames, frames_to_render, delay_seconds,
                    delay_timestamp_seconds);
    }

    fifo_->Pull(output_bus_.get(), number_of_frames);

  } else {
    // Fill the FIFO.
    if (worklet_task_runner_) {
      // Use the dual-thread rendering if the AudioWorklet is activated.
      const size_t frames_to_render =
          fifo_->PullAndUpdateEarmark(output_bus_.get(), number_of_frames);
      PostCrossThreadTask(
          *worklet_task_runner_, FROM_HERE,
          CrossThreadBindOnce(&AudioDestination::RequestRender,
                              WrapRefCounted(this), number_of_frames,
                              frames_to_render, delay_seconds,
                              delay_timestamp_seconds));
    } else {
      // Otherwise use the single-thread rendering.
      const size_t frames_to_render =
          fifo_->Pull(output_bus_.get(), number_of_frames);
      RequestRender(number_of_frames, frames_to_render, delay_seconds,
                    delay_timestamp_seconds);
    }
  }

  TRACE_EVENT_END2("webaudio", "AudioDestination::Render", "timestamp (s)",
                   delay_timestamp_seconds, "delay (s)", delay_seconds);

  return number_of_frames;
}

void AudioDestination::OnRenderError() {
  // TODO(crbug.com/1406088)
}

void AudioDestination::Start() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::Start");
  SendLogMessage(String::Format("%s", __func__));

  if (device_state_ != DeviceState::kStopped) {
    return;
  }
  web_audio_device_->Start();
  SetDeviceState(DeviceState::kRunning);
}

void AudioDestination::Stop() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::Stop");
  SendLogMessage(String::Format("%s", __func__));

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
  SendLogMessage(String::Format("%s", __func__));

  if (device_state_ != DeviceState::kRunning) {
    return;
  }
  web_audio_device_->Pause();
  SetDeviceState(DeviceState::kPaused);
}

void AudioDestination::Resume() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioDestination::Resume");
  SendLogMessage(String::Format("%s", __func__));

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
  SendLogMessage(String::Format("%s", __func__));

  if (device_state_ != DeviceState::kStopped) {
    return;
  }

  SetWorkletTaskRunner(worklet_task_runner);
  web_audio_device_->Start();
  SetDeviceState(DeviceState::kRunning);
}

bool AudioDestination::IsPlaying() {
  DCHECK(IsMainThread());
  base::AutoLock locker(device_state_lock_);
  return device_state_ == DeviceState::kRunning;
}

double AudioDestination::SampleRate() const {
  return context_sample_rate_;
}

uint32_t AudioDestination::CallbackBufferSize() const {
  DCHECK(IsMainThread());
  return callback_buffer_size_;
}

int AudioDestination::FramesPerBuffer() const {
  DCHECK(IsMainThread());
  return web_audio_device_->FramesPerBuffer();
}

uint32_t AudioDestination::MaxChannelCount() {
  return web_audio_device_->MaxChannelCount();
}

void AudioDestination::SetDetectSilence(bool detect_silence) {
  DCHECK(IsMainThread());
  TRACE_EVENT1("webaudio", "AudioDestination::SetDetectSilence",
               "detect_silence", detect_silence);
  SendLogMessage(
      String::Format("%s({detect_silence=%d})", __func__, detect_silence));

  web_audio_device_->SetDetectSilence(detect_silence);
}

unsigned AudioDestination::RenderQuantumFrames() const {
  return render_quantum_frames_;
}

AudioDestination::AudioDestination(
    AudioIOCallback& callback,
    const WebAudioSinkDescriptor& sink_descriptor,
    unsigned number_of_output_channels,
    const WebAudioLatencyHint& latency_hint,
    absl::optional<float> context_sample_rate,
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
      fifo_(std::make_unique<PushPullFIFO>(number_of_output_channels,
                                           kFIFOSize,
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

  SendLogMessage(String::Format("%s({output_channels=%u})", __func__,
                                number_of_output_channels));
  SendLogMessage(
      String::Format("%s => (FIFO size=%u bytes)", __func__, fifo_->length()));

  SendLogMessage(String::Format("%s => (device callback buffer size=%u frames)",
                                __func__, callback_buffer_size_));
  SendLogMessage(String::Format("%s => (device sample rate=%.0f Hz)", __func__,
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

  // Check if the requested buffer size is too large.
  DCHECK_LE(callback_buffer_size_ + render_quantum_frames, kFIFOSize);

  double scale_factor = 1.0;

  if (context_sample_rate_ != web_audio_device_->SampleRate()) {
    scale_factor = context_sample_rate_ / web_audio_device_->SampleRate();
    SendLogMessage(String::Format("%s => (resampling from %0.f Hz to %0.f Hz)",
                                  __func__, context_sample_rate.value(),
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
    SendLogMessage(String::Format(
        "%s => (no resampling: context sample rate set to %0.f Hz)", __func__,
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

void AudioDestination::RequestRenderWait(size_t frames_requested,
                                         size_t frames_to_render,
                                         double delay,
                                         double delay_timestamp) {
  RequestRender(frames_requested, frames_to_render, delay, delay_timestamp);
  output_buffer_bypass_wait_event_.Signal();
}

void AudioDestination::RequestRender(size_t frames_requested,
                                     size_t frames_to_render,
                                     double delay,
                                     double delay_timestamp) {
  TRACE_EVENT2("webaudio", "AudioDestination::RequestRender",
               "frames_to_render", frames_to_render, "timestamp (s)",
               delay_timestamp);

  base::AutoTryLock locker(device_state_lock_);

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
    SendLogMessage(String::Format("%s => (rendering is now alive)", __func__));
  }

  output_position_.position =
      frames_elapsed_ / static_cast<double>(web_audio_device_->SampleRate()) -
      delay;
  output_position_.timestamp = delay_timestamp;
  output_position_.hardware_output_latency = delay;
  const base::TimeTicks callback_request = base::TimeTicks::Now();

  for (size_t pushed_frames = 0; pushed_frames < frames_to_render;
       pushed_frames += RenderQuantumFrames()) {
    // If platform buffer is more than two times longer than
    // `RenderQuantumFrames` we do not want output position to get stuck so we
    // promote it using the elapsed time from the moment it was initially
    // obtained.
    if (callback_buffer_size_ > RenderQuantumFrames() * 2) {
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

    if (resampler_) {
      resampler_->ResampleInternal(RenderQuantumFrames(), resampler_bus_.get());
    } else {
      // Process WebAudio graph and push the rendered output to FIFO.
      callback_->Render(render_bus_.get(), RenderQuantumFrames(),
                        output_position_, metric_reporter_.GetMetric());
    }

    fifo_->Push(render_bus_.get());
  }

  frames_elapsed_ += frames_requested;

  metric_reporter_.EndTrace();
}

void AudioDestination::ProvideResamplerInput(int resampler_frame_delay,
                                             AudioBus* dest) {
  callback_->Render(dest, RenderQuantumFrames(), output_position_,
                    metric_reporter_.GetMetric());
}

void AudioDestination::SendLogMessage(const String& message) const {
  WebRtcLogMessage(String::Format("[WA]AD::%s [state=%s]",
                                  message.Utf8().c_str(),
                                  DeviceStateToString(device_state_))
                       .Utf8());
}

media::OutputDeviceStatus AudioDestination::CreateSinkAndGetDeviceStatus() {
  TRACE_EVENT0("webaudio", "AudioDestination::CreateSinkAndGetDeviceStatus");
  return web_audio_device_->CreateSinkAndGetDeviceStatus();
}

}  // namespace blink
