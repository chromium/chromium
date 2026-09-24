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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_H_

#include "third_party/blink/renderer/modules/webaudio/periodic_wave_handler.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class BaseAudioContext;
class ExceptionState;
class PeriodicWaveOptions;

class PeriodicWave final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PeriodicWave* CreateSine(float sample_rate);
  static PeriodicWave* CreateSquare(float sample_rate);
  static PeriodicWave* CreateSawtooth(float sample_rate);
  static PeriodicWave* CreateTriangle(float sample_rate);

  // Creates an arbitrary periodic wave given the frequency components (Fourier
  // coefficients).
  static PeriodicWave* Create(BaseAudioContext&,
                              const Vector<float>& real,
                              const Vector<float>& imag,
                              bool normalize,
                              ExceptionState&);

  static PeriodicWave* Create(BaseAudioContext*,
                              const PeriodicWaveOptions*,
                              ExceptionState&);

  explicit PeriodicWave(float sample_rate);
  ~PeriodicWave() final = default;

  void Trace(Visitor*) const final;

  PeriodicWaveHandler* handler() { return periodic_wave_handler_.Get(); }

 private:
  const Member<PeriodicWaveHandler> periodic_wave_handler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_H_
