/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_periodic_wave_options.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_handler.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

PeriodicWave* PeriodicWave::Create(BaseAudioContext& context,
                                   const Vector<float>& real,
                                   const Vector<float>& imag,
                                   bool disable_normalization,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (real.size() != imag.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        StrCat({"length of real array (", String::Number(real.size()),
                ") and length of imaginary array (",
                String::Number(imag.size()), ") must match."}));
    return nullptr;
  }

  if (real.size() < 2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMinimumBound("length of the real array",
                                                    real.size(), 2u));
    return nullptr;
  }

  if (imag.size() < 2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMinimumBound("length of the imag array",
                                                    imag.size(), 2u));
    return nullptr;
  }

  PeriodicWave* periodic_wave =
      MakeGarbageCollected<PeriodicWave>(context.sampleRate());
  return periodic_wave->handler()->CreateBandLimitedTables(
             base::span<const float>(real), base::span<const float>(imag),
             disable_normalization)
             ? periodic_wave
             : nullptr;
}

PeriodicWave* PeriodicWave::Create(BaseAudioContext* context,
                                   const PeriodicWaveOptions* options,
                                   ExceptionState& exception_state) {
  bool normalize = options->disableNormalization();

  Vector<float> real_coef;
  Vector<float> imag_coef;

  if (options->hasReal()) {
    real_coef = options->real();
    if (options->hasImag()) {
      imag_coef = options->imag();
    } else {
      imag_coef.resize(real_coef.size());
    }
  } else if (options->hasImag()) {
    // `real()` not given, but we have `imag()`.
    imag_coef = options->imag();
    real_coef.resize(imag_coef.size());
  } else {
    // Neither `real()` nor `imag()` given.  Return an object that would
    // generate a sine wave, which means real = [0,0], and imag = [0, 1]
    real_coef.resize(2);
    imag_coef.resize(2);
    imag_coef[1] = 1;
  }

  return Create(*context, real_coef, imag_coef, normalize, exception_state);
}

PeriodicWave* PeriodicWave::CreateSine(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  return periodic_wave->handler()->GenerateBasicWaveform(
             OscillatorHandler::SINE)
             ? periodic_wave
             : nullptr;
}

PeriodicWave* PeriodicWave::CreateSquare(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  return periodic_wave->handler()->GenerateBasicWaveform(
             OscillatorHandler::SQUARE)
             ? periodic_wave
             : nullptr;
}

PeriodicWave* PeriodicWave::CreateSawtooth(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  return periodic_wave->handler()->GenerateBasicWaveform(
             OscillatorHandler::SAWTOOTH)
             ? periodic_wave
             : nullptr;
}

PeriodicWave* PeriodicWave::CreateTriangle(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  return periodic_wave->handler()->GenerateBasicWaveform(
             OscillatorHandler::TRIANGLE)
             ? periodic_wave
             : nullptr;
}

PeriodicWave::PeriodicWave(float sample_rate)
    : periodic_wave_handler_(
          MakeGarbageCollected<PeriodicWaveHandler>(sample_rate)) {}

void PeriodicWave::Trace(Visitor* visitor) const {
  visitor->Trace(periodic_wave_handler_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
