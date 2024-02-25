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
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_audio_bus.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"

namespace blink {

void WebAudioBus::Initialize(unsigned number_of_channels,
                             size_t length,
                             double sample_rate) {
  scoped_refptr<AudioBus> audio_bus = AudioBus::Create(
      number_of_channels, base::checked_cast<wtf_size_t>(length));
  audio_bus->SetSampleRate(sample_rate);

  if (private_)
    private_->Release();

  audio_bus->AddRef();
  private_ = audio_bus.get();
}

void WebAudioBus::ResizeSmaller(size_t new_length) {
  DCHECK(private_);
  if (private_) {
    DCHECK_LE(new_length, length());
    private_->ResizeSmaller(static_cast<wtf_size_t>(new_length));
  }
}

void WebAudioBus::Reset() {
  if (private_) {
    private_->Release();
    private_ = nullptr;
  }
}

unsigned WebAudioBus::NumberOfChannels() const {
  if (!private_)
    return 0;
  return private_->NumberOfChannels();
}

size_t WebAudioBus::length() const {
  if (!private_)
    return 0;
  return private_->length();
}

double WebAudioBus::SampleRate() const {
  if (!private_)
    return 0;
  return private_->SampleRate();
}

float* WebAudioBus::ChannelData(unsigned channel_index) {
  if (!private_)
    return nullptr;
  DCHECK_LT(channel_index, NumberOfChannels());
  return private_->Channel(channel_index)->MutableData();
}

scoped_refptr<AudioBus> WebAudioBus::Release() {
  scoped_refptr<AudioBus> audio_bus(private_.get());
  private_->Release();
  private_ = nullptr;
  return audio_bus;
}

}  // namespace blink
