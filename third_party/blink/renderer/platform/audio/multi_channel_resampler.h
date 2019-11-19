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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MULTI_CHANNEL_RESAMPLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MULTI_CHANNEL_RESAMPLER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/audio/sinc_resampler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AudioBus;

class PLATFORM_EXPORT MultiChannelResampler {
  USING_FAST_MALLOC(MultiChannelResampler);

 public:
  MultiChannelResampler(double scale_factor, unsigned number_of_channels);

  // Process given AudioSourceProvider for streaming applications.
  void Process(AudioSourceProvider*,
               AudioBus* destination,
               uint32_t frames_to_process);

 private:
  // FIXME: the mac port can have a more highly optimized implementation based
  // on CoreAudio instead of SincResampler. For now the default implementation
  // will be used on all ports.
  // https://bugs.webkit.org/show_bug.cgi?id=75118

  // Each channel will be resampled using a high-quality SincResampler.
  Vector<std::unique_ptr<SincResampler>> kernels_;

  unsigned number_of_channels_;

  DISALLOW_COPY_AND_ASSIGN(MultiChannelResampler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MULTI_CHANNEL_RESAMPLER_H_
