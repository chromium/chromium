// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/audio_hash.h"

#include <cmath>
#include <numbers>
#include <sstream>

#include "base/strings/stringprintf.h"
#include "media/base/audio_bus.h"

namespace media {

AudioHash::AudioHash()
    : audio_hash_(),
      sample_count_(0) {
}

AudioHash::~AudioHash() = default;

void AudioHash::Update(const AudioBus* audio_bus, int frames) {
  // Use uint32_t to ensure overflow is a defined operation.
  for (uint32_t ch = 0; ch < static_cast<uint32_t>(audio_bus->channels());
       ++ch) {
    const float* channel = audio_bus->channel(ch);
    for (uint32_t i = 0; i < static_cast<uint32_t>(frames); ++i) {
      const uint32_t kSampleIndex = sample_count_ + i;
      const uint32_t kHashIndex =
          (kSampleIndex * (ch + 1)) % std::size(audio_hash_);

      // Mix in a sine wave with the result so we ensure that sequences of empty
      // buffers don't result in an empty hash.
      if (ch == 0) {
        audio_hash_[kHashIndex] +=
            channel[i] +
            std::sin(2.0 * std::numbers::pi * std::numbers::pi * kSampleIndex);
      } else {
        audio_hash_[kHashIndex] += channel[i];
      }
    }
  }

  sample_count_ += static_cast<uint32_t>(frames);
}

std::string AudioHash::ToString() const {
  std::string result;
  for (size_t i = 0; i < std::size(audio_hash_); ++i)
    result += base::StringPrintf("%.2f,", audio_hash_[i]);
  return result;
}

bool AudioHash::IsEquivalent(const std::string& other, double tolerance) const {
  float other_hash;
  char comma;

  std::stringstream is(other);
  for (size_t i = 0; i < std::size(audio_hash_); ++i) {
    is >> other_hash >> comma;
    if (std::fabs(audio_hash_[i] - other_hash) > tolerance)
      return false;
  }
  return true;
}

}  // namespace media
