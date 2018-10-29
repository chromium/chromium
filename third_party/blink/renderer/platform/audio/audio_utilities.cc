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
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace audio_utilities {

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
  return 1 - exp(-1 / (sample_rate * time_constant));
}

size_t TimeToSampleFrame(double time, double sample_rate) {
  DCHECK_GE(time, 0);
  double frame = round(time * sample_rate);

  // Just return the largest possible size_t value if necessary.
  if (frame >= std::numeric_limits<size_t>::max()) {
    return std::numeric_limits<size_t>::max();
  }

  return static_cast<size_t>(frame);
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
  // <video> tags support sample rates up 384 kHz so audio context
  // should too.
  return 384000;
}

bool IsPowerOfTwo(size_t x) {
  // From Hacker's Delight.  x & (x - 1) turns off (zeroes) the
  // rightmost 1-bit in the word x.  If x is a power of two, then the
  // result is, of course, 0.
  return x > 0 && ((x & (x - 1)) == 0);
}

}  // namespace audio_utilities

}  // namespace blink
