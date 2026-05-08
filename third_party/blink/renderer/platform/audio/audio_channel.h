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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_CHANNEL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_CHANNEL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// An AudioChannel represents a buffer of non-interleaved floating-point audio
// samples.
// The PCM samples are normally assumed to be in a nominal range -1.0 -> +1.0
class PLATFORM_EXPORT AudioChannel final {
 public:
  // Memory can be externally referenced, or can be internally allocated with an
  // AudioFloatArray.

  // Manage storage for us.
  explicit AudioChannel(uint32_t length) : silent_(true) {
    CHECK(TryAllocate(length));
  }

  // A "blank" audio channel -- must call Set() before it's useful...
  AudioChannel() : silent_(true) {}

  // Methods for internal allocation.
  bool TryAllocate(uint32_t length);

  void Set(base::span<float> storage) {
    mem_buffer_.reset();  // cleanup managed storage
    data_span_ = storage;
    silent_ = false;
  }

  // How many sample-frames do we contain?
  uint32_t length() const { return data_span_.size(); }

  // ResizeSmaller() can only be called with a new length <= the current length.
  // The data stored in the bus will remain undisturbed.
  void ResizeSmaller(uint32_t new_length);

  // Direct access to PCM sample data. Non-const accessor clears silent flag.
  float* MutableData() {
    ClearSilentFlag();
    return data_span_.data();
  }

  const float* Data() const { return data_span_.data(); }

  base::span<float> MutableSpan() {
    ClearSilentFlag();
    return data_span_;
  }

  base::span<const float> Span() const { return data_span_; }

  // Zeroes out all sample values in buffer.
  void Zero() {
    if (!silent_) {
      std::ranges::fill(MutableSpan(), 0.f);
      // Set silent flag after calling `MutableSpan()` so that it is not cleared
      // again.
      silent_ = true;
    }
  }

  // Clears the silent flag.
  void ClearSilentFlag() { silent_ = false; }

  bool IsSilent() const { return silent_; }

  // Scales all samples by the same amount.
  void Scale(float scale);

  // A simple memcpy() from the source channel
  void CopyFrom(const AudioChannel* source_channel);

  // Copies the given range from the source channel.
  void CopyFromRange(const AudioChannel* source_channel,
                     unsigned start_frame,
                     unsigned end_frame);

  // Sums (with unity gain) from the source channel.
  void SumFrom(const AudioChannel* source_channel);

  // Returns maximum absolute value (useful for normalization).
  float MaxAbsValue() const;

 private:
  std::unique_ptr<AudioFloatArray> mem_buffer_;
  base::raw_span<float> data_span_;
  bool silent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_CHANNEL_H_
