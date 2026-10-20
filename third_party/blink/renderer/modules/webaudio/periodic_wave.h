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

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave_handler.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class BaseAudioContext;
class ExceptionState;
class PeriodicWaveOptions;

// PeriodicWave represents a periodic audio waveform defined by its Fourier
// coefficients, used by OscillatorNode to shape its output. It is a
// main-thread ScriptWrappable wrapper around PeriodicWaveHandler.
class MODULES_EXPORT PeriodicWave final : public ScriptWrappable {
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
                              bool disable_normalization,
                              ExceptionState&);

  static PeriodicWave* Create(BaseAudioContext*,
                              const PeriodicWaveOptions*,
                              ExceptionState&);

  explicit PeriodicWave(scoped_refptr<const PeriodicWaveHandler> handler);
  ~PeriodicWave() final;

  scoped_refptr<const PeriodicWaveHandler> handler() const {
    return periodic_wave_handler_;
  }

 private:
  static PeriodicWave* CreateBasicWaveform(float sample_rate, int shape);

  const scoped_refptr<const PeriodicWaveHandler> periodic_wave_handler_;

  // Held by PeriodicWave rather than PeriodicWaveHandler so that V8 external
  // memory accounting remains associated with the script wrapper on the main
  // thread while PeriodicWaveHandler remains a pure audio data structure.
  V8ExternalMemoryAccounter external_memory_accounter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_H_
