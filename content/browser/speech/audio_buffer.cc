// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/audio_buffer.h"

#include "base/check_op.h"

namespace content {

AudioChunk::AudioChunk(int bytes_per_sample)
    : bytes_per_sample_(bytes_per_sample) {
}

AudioChunk::AudioChunk(size_t length, int bytes_per_sample)
    : data_string_(length, '\0'), bytes_per_sample_(bytes_per_sample) {
  DCHECK_EQ(length % bytes_per_sample, 0U);
}

AudioChunk::AudioChunk(const uint8_t* data, size_t length, int bytes_per_sample)
    : data_string_(reinterpret_cast<const char*>(data), length),
      bytes_per_sample_(bytes_per_sample) {
  DCHECK_EQ(length % bytes_per_sample, 0U);
}

bool AudioChunk::IsEmpty() const {
  return data_string_.empty();
}

size_t AudioChunk::NumSamples() const {
  return data_string_.size() / bytes_per_sample_;
}

const std::string& AudioChunk::AsString() const {
  return data_string_;
}

int16_t AudioChunk::GetSample16(size_t index) const {
  DCHECK(index < (data_string_.size() / sizeof(int16_t)));
  return SamplesData16()[index];
}

const int16_t* AudioChunk::SamplesData16() const {
  return reinterpret_cast<const int16_t*>(data_string_.data());
}

AudioBuffer::AudioBuffer(int bytes_per_sample)
    : bytes_per_sample_(bytes_per_sample) {
  DCHECK(bytes_per_sample == 1 ||
         bytes_per_sample == 2 ||
         bytes_per_sample == 4);
}

AudioBuffer::~AudioBuffer() {
  Clear();
}

void AudioBuffer::Enqueue(const uint8_t* data, size_t length) {
  chunks_.push_back(new AudioChunk(data, length, bytes_per_sample_));
}

scoped_refptr<AudioChunk> AudioBuffer::DequeueSingleChunk() {
  DCHECK(!chunks_.empty());
  scoped_refptr<AudioChunk> chunk(chunks_.front());
  chunks_.pop_front();
  return chunk;
}

scoped_refptr<AudioChunk> AudioBuffer::DequeueAll() {
  size_t resulting_length = 0;
  ChunksContainer::const_iterator it;
  // In order to improve performance, calulate in advance the total length
  // and then copy the chunks.
  for (it = chunks_.begin(); it != chunks_.end(); ++it) {
    resulting_length += (*it)->AsString().length();
  }
  scoped_refptr<AudioChunk> chunk(
      new AudioChunk(resulting_length, bytes_per_sample_));
  uint8_t* dest = chunk->writable_data();
  for (it = chunks_.begin(); it != chunks_.end(); ++it) {
    memcpy(dest, (*it)->AsString().data(), (*it)->AsString().length());
    dest += (*it)->AsString().length();
  }
  Clear();
  return chunk;
}

void AudioBuffer::Clear() {
  chunks_.clear();
}

bool AudioBuffer::IsEmpty() const {
  return chunks_.empty();
}

}  // namespace content
