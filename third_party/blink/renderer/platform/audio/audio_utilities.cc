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

#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

#include <sstream>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/fdlibm/ieee754.h"

namespace blink::audio_utilities {

float DecibelsToLinear(float decibels) {
  return powf(10, 0.05f * decibels);
}

float LinearToDecibels(float linear) {
  DCHECK_GE(linear, 0);

  return 20 * log10f(linear);
}

double DiscreteTimeConstantForSampleRate(double time_constant,
                                         double sample_rate) {
  // From the WebAudio spec, the formula for setTargetAtTime is
  //
  //   v(t) = V1 + (V0 - V1)*exp(-t/tau)
  //
  // where tau is the time constant, V1 is the target value and V0 is
  // the starting value.
  //
  // Rewrite this as
  //
  //   v(t) = V0 + (V1 - V0)*(1-exp(-t/tau))
  //
  // The implementation of setTargetAtTime uses this form.  So at the
  // sample points, we have
  //
  //   v(n/Fs) = V0 + (V1 - V0)*(1-exp(-n/(Fs*tau)))
  //
  // where Fs is the sample rate of the sampled systme.  Thus, the
  // discrete time constant is
  //
  //   1 - exp(-1/(Fs*tau)
  return 1 - fdlibm::exp(-1 / (sample_rate * time_constant));
}

size_t TimeToSampleFrame(double time,
                         double sample_rate,
                         enum SampleFrameRounding rounding_mode) {
  DCHECK_GE(time, 0);

  // To compute the desired frame number, we pretend we're actually running the
  // context at a much higher sample rate (by a factor of |oversample_factor|).
  // Round this to get the nearest frame number at the higher rate.  Then
  // convert back to the original rate to get a new frame number that may not be
  // an integer.  Then use the specified |rounding_mode| to round this to the
  // integer frame number that we need.
  //
  // Doing this partially solves the issue where Fs * (k / Fs) != k when doing
  // floating point arithmtic for integer k and Fs is the sample rate.  By
  // oversampling and rounding, we'll get k back most of the time.
  //
  // The oversampling factor MUST be a power of two so as not to introduce
  // additional round-off in computing the oversample frame number.
  const double oversample_factor = 1024;
  double frame =
      round(time * sample_rate * oversample_factor) / oversample_factor;

  switch (rounding_mode) {
    case kRoundToNearest:
      frame = round(frame);
      break;
    case kRoundDown:
      frame = floor(frame);
      break;
    case kRoundUp:
      frame = ceil(frame);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // Just return the largest possible size_t value if necessary.
  if (frame >= std::numeric_limits<size_t>::max()) {
    return std::numeric_limits<size_t>::max();
  }

  return static_cast<size_t>(frame);
}

base::TimeDelta FramesToTime(int64_t frames, float sample_rate) {
  CHECK_GT(sample_rate, 0.f);
  return base::Microseconds(static_cast<int64_t>(
      frames * base::Time::kMicrosecondsPerSecond / sample_rate));
}

bool IsValidAudioBufferSampleRate(float sample_rate) {
  return sample_rate >= MinAudioBufferSampleRate() &&
         sample_rate <= MaxAudioBufferSampleRate();
}

float MinAudioBufferSampleRate() {
  // crbug.com/344375
  return 3000;
}

float MaxAudioBufferSampleRate() {
  // <video> tags support sample rates up 768 kHz so audio context
  // should too.
  return 768000;
}

const std::string GetSinkIdForTracing(
    blink::WebAudioSinkDescriptor sink_descriptor) {
  std::string sink_id;
  if (sink_descriptor.Type() == blink::WebAudioSinkDescriptor::kAudible) {
    sink_id = sink_descriptor.SinkId() == "" ?
        "DEFAULT SINK" : sink_descriptor.SinkId().Utf8();
  } else {
    sink_id = "SILENT SINK";
  }
  return sink_id;
}

const std::string GetSinkInfoForTracing(
    blink::WebAudioSinkDescriptor sink_descriptor,
    blink::WebAudioLatencyHint latency_hint,
    int channel_count,
    float sample_rate,
    int callback_buffer_size) {
  std::ostringstream s;

  s << "sink info: " << GetSinkIdForTracing(sink_descriptor);

  std::string latency_info;
  switch (latency_hint.Category()) {
    case WebAudioLatencyHint::kCategoryInteractive:
      latency_info = "interactive";
      break;
    case WebAudioLatencyHint::kCategoryBalanced:
      latency_info = "balanced";
      break;
    case WebAudioLatencyHint::kCategoryPlayback:
      latency_info = "playback";
      break;
    case WebAudioLatencyHint::kCategoryExact:
      latency_info = "exact";
      break;
    case WebAudioLatencyHint::kLastValue:
      latency_info = "invalid";
      break;
  }
  s << ", latency hint: " << latency_info;

  if (latency_hint.Category() == WebAudioLatencyHint::kCategoryExact) {
    s << " (" << latency_hint.Seconds() << "s)";
  }

  s << ", channel count: " << channel_count
    << ", sample rate: " << sample_rate
    << ", callback buffer size: " << callback_buffer_size;

  return s.str();
}

const std::string GetDeviceEnumerationForTracing(
    const Vector<WebMediaDeviceInfo>& device_infos) {
  std::ostringstream s;

  for (auto device_info : device_infos) {
    s << "{ label: " << device_info.label
      << ", device_id: " << device_info.device_id
      << ", group_id: " << device_info.group_id << " }";
  }

  return s.str().empty() ? "EMPTY" : s.str();
}

}  // namespace blink::audio_utilities
