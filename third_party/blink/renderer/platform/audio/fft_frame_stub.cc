/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

// FFTFrame stub implementation to avoid link errors during bringup

#include "build/build_config.h"

#if !BUILDFLAG(IS_MAC) && !defined(WTF_USE_WEBAUDIO_PFFFT)

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

namespace blink {

// Normal constructor: allocates for a given fftSize.
FFTFrame::FFTFrame(unsigned /*fftSize*/) : fft_size_(0), log2fft_size_(0) {
  NOTREACHED_IN_MIGRATION();
}

// Creates a blank/empty frame (interpolate() must later be called).
FFTFrame::FFTFrame() : fft_size_(0), log2fft_size_(0) {
  NOTREACHED_IN_MIGRATION();
}

// Copy constructor.
FFTFrame::FFTFrame(const FFTFrame& frame)
    : fft_size_(frame.fft_size_), log2fft_size_(frame.log2fft_size_) {
  NOTREACHED_IN_MIGRATION();
}

FFTFrame::~FFTFrame() {
  NOTREACHED_IN_MIGRATION();
}

void FFTFrame::DoFFT(const float* data) {
  NOTREACHED_IN_MIGRATION();
}

void FFTFrame::DoInverseFFT(float* data) {
  NOTREACHED_IN_MIGRATION();
}

void FFTFrame::Initialize() {}

void FFTFrame::Cleanup() {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace blink

#endif  // !BUILDFLAG(IS_MAC) && !defined(WTF_USE_WEBAUDIO_PFFFT)
