// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/audio_buffer.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_AudioBuffer_0_1>() {
  return PPB_AUDIOBUFFER_INTERFACE_0_1;
}

}  // namespace

AudioBuffer::AudioBuffer() {
}

AudioBuffer::AudioBuffer(const AudioBuffer& other) : Resource(other) {
}

AudioBuffer::AudioBuffer(const Resource& resource) : Resource(resource) {
}

AudioBuffer::AudioBuffer(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

AudioBuffer::~AudioBuffer() {
}

PP_TimeDelta AudioBuffer::GetTimestamp() const {
  if (has_interface<PPB_AudioBuffer_0_1>())
    return get_interface<PPB_AudioBuffer_0_1>()->GetTimestamp(pp_resource());
  return 0.0;
}

void AudioBuffer::SetTimestamp(PP_TimeDelta timestamp) {
  if (has_interface<PPB_AudioBuffer_0_1>()) {
    get_interface<PPB_AudioBuffer_0_1>()->SetTimestamp(pp_resource(),
                                                       timestamp);
  }
}

PP_AudioBuffer_SampleRate AudioBuffer::GetSampleRate() const {
  if (has_interface<PPB_AudioBuffer_0_1>())
    return get_interface<PPB_AudioBuffer_0_1>()->GetSampleRate(pp_resource());
  return PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN;
}

PP_AudioBuffer_SampleSize AudioBuffer::GetSampleSize() const {
  if (has_interface<PPB_AudioBuffer_0_1>())
    return get_interface<PPB_AudioBuffer_0_1>()->GetSampleSize(pp_resource());
  return PP_AUDIOBUFFER_SAMPLESIZE_UNKNOWN;
}

uint32_t AudioBuffer::GetNumberOfChannels() const {
  if (has_interface<PPB_AudioBuffer_0_1>()) {
    return get_interface<PPB_AudioBuffer_0_1>()->GetNumberOfChannels(
        pp_resource());
  }
  return 0;
}

uint32_t AudioBuffer::GetNumberOfSamples() const {
  if (has_interface<PPB_AudioBuffer_0_1>()) {
    return get_interface<PPB_AudioBuffer_0_1>()->GetNumberOfSamples(
        pp_resource());
  }
  return 0;
}

void* AudioBuffer::GetDataBuffer() {
  if (has_interface<PPB_AudioBuffer_0_1>())
    return get_interface<PPB_AudioBuffer_0_1>()->GetDataBuffer(pp_resource());
  return NULL;
}

uint32_t AudioBuffer::GetDataBufferSize() const {
  if (has_interface<PPB_AudioBuffer_0_1>()) {
    return get_interface<PPB_AudioBuffer_0_1>()->GetDataBufferSize(
        pp_resource());
  }
  return 0;
}

}  // namespace pp
