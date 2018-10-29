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

#include "third_party/blink/renderer/platform/audio/audio_channel.h"

#include <math.h>
#include <algorithm>
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

using namespace vector_math;

void AudioChannel::ResizeSmaller(size_t new_length) {
  DCHECK_LE(new_length, length_);
  if (new_length <= length_)
    length_ = new_length;
}

void AudioChannel::Scale(float scale) {
  if (IsSilent())
    return;

  Vsmul(Data(), 1, &scale, MutableData(), 1, length());
}

void AudioChannel::CopyFrom(const AudioChannel* source_channel) {
  bool is_safe = (source_channel && source_channel->length() >= length());
  DCHECK(is_safe);
  if (!is_safe)
    return;

  if (source_channel->IsSilent()) {
    Zero();
    return;
  }
  memcpy(MutableData(), source_channel->Data(),
         base::CheckMul(sizeof(float), length()).ValueOrDie());
}

void AudioChannel::CopyFromRange(const AudioChannel* source_channel,
                                 unsigned start_frame,
                                 unsigned end_frame) {
  // Check that range is safe for reading from sourceChannel.
  bool is_range_safe = source_channel && start_frame < end_frame &&
                       end_frame <= source_channel->length();
  DCHECK(is_range_safe);
  if (!is_range_safe)
    return;

  if (source_channel->IsSilent() && IsSilent())
    return;

  // Check that this channel has enough space.
  size_t range_length = end_frame - start_frame;
  bool is_range_length_safe = range_length <= length();
  DCHECK(is_range_length_safe);
  if (!is_range_length_safe)
    return;

  const float* source = source_channel->Data();
  float* destination = MutableData();

  const size_t safe_length =
      base::CheckMul(sizeof(float), range_length).ValueOrDie();
  if (source_channel->IsSilent()) {
    if (range_length == length())
      Zero();
    else
      memset(destination, 0, safe_length);
  } else {
    memcpy(destination, source + start_frame, safe_length);
  }
}

void AudioChannel::SumFrom(const AudioChannel* source_channel) {
  bool is_safe = source_channel && source_channel->length() >= length();
  DCHECK(is_safe);
  if (!is_safe)
    return;

  if (source_channel->IsSilent())
    return;

  if (IsSilent())
    CopyFrom(source_channel);
  else
    Vadd(Data(), 1, source_channel->Data(), 1, MutableData(), 1, length());
}

float AudioChannel::MaxAbsValue() const {
  if (IsSilent())
    return 0;

  float max = 0;

  Vmaxmgv(Data(), 1, &max, length());

  return max;
}

}  // namespace blink
