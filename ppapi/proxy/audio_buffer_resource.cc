// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_buffer_resource.h"

#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

AudioBufferResource::AudioBufferResource(PP_Instance instance,
                                       int32_t index,
                                       MediaStreamBuffer* buffer)
    : Resource(OBJECT_IS_PROXY, instance),
      index_(index),
      buffer_(buffer) {
  DCHECK_EQ(buffer_->header.type, MediaStreamBuffer::TYPE_AUDIO);
}

AudioBufferResource::~AudioBufferResource() {
  CHECK(!buffer_) << "An unused (or unrecycled) buffer is destroyed.";
}

thunk::PPB_AudioBuffer_API* AudioBufferResource::AsPPB_AudioBuffer_API() {
  return this;
}

PP_TimeDelta AudioBufferResource::GetTimestamp() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0.0;
  }
  return buffer_->audio.timestamp;
}

void AudioBufferResource::SetTimestamp(PP_TimeDelta timestamp) {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return;
  }
  buffer_->audio.timestamp = timestamp;
}

PP_AudioBuffer_SampleRate AudioBufferResource::GetSampleRate() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN;
  }
  return buffer_->audio.sample_rate;
}

PP_AudioBuffer_SampleSize AudioBufferResource::GetSampleSize() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return PP_AUDIOBUFFER_SAMPLESIZE_UNKNOWN;
  }
  return PP_AUDIOBUFFER_SAMPLESIZE_16_BITS;
}

uint32_t AudioBufferResource::GetNumberOfChannels() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0;
  }
  return buffer_->audio.number_of_channels;
}

uint32_t AudioBufferResource::GetNumberOfSamples() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0;
  }
  return buffer_->audio.number_of_samples;
}

void* AudioBufferResource::GetDataBuffer() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return NULL;
  }
  return buffer_->audio.data;
}

uint32_t AudioBufferResource::GetDataBufferSize() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0;
  }
  return buffer_->audio.data_size;
}

MediaStreamBuffer* AudioBufferResource::GetBuffer() {
  return buffer_;
}

int32_t AudioBufferResource::GetBufferIndex() {
  return index_;
}

void AudioBufferResource::Invalidate() {
  DCHECK(buffer_);
  DCHECK_GE(index_, 0);
  buffer_ = NULL;
  index_ = -1;
}

}  // namespace proxy
}  // namespace ppapi
