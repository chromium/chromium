// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_encoder_resource.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/not_fatal_until.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/proxy/audio_buffer_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {

AudioEncoderResource::AudioEncoderResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance),
      encoder_last_error_(PP_ERROR_FAILED),
      initialized_(false),
      audio_buffer_manager_(this),
      bitstream_buffer_manager_(this) {
  SendCreate(RENDERER, PpapiHostMsg_AudioEncoder_Create());
}

AudioEncoderResource::~AudioEncoderResource() {
}

thunk::PPB_AudioEncoder_API* AudioEncoderResource::AsPPB_AudioEncoder_API() {
  return this;
}

int32_t AudioEncoderResource::GetSupportedProfiles(
    const PP_ArrayOutput& output,
    const scoped_refptr<TrackedCallback>& callback) {
  if (TrackedCallback::IsPending(get_supported_profiles_callback_))
    return PP_ERROR_INPROGRESS;

  get_supported_profiles_callback_ = callback;
  Call<PpapiPluginMsg_AudioEncoder_GetSupportedProfilesReply>(
      RENDERER, PpapiHostMsg_AudioEncoder_GetSupportedProfiles(),
      base::BindOnce(
          &AudioEncoderResource::OnPluginMsgGetSupportedProfilesReply, this,
          output));
  return PP_OK_COMPLETIONPENDING;
}

int32_t AudioEncoderResource::Initialize(
    uint32_t channels,
    PP_AudioBuffer_SampleRate input_sample_rate,
    PP_AudioBuffer_SampleSize input_sample_size,
    PP_AudioProfile output_profile,
    uint32_t initial_bitrate,
    PP_HardwareAcceleration acceleration,
    const scoped_refptr<TrackedCallback>& callback) {
  if (initialized_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(initialize_callback_))
    return PP_ERROR_INPROGRESS;

  initialize_callback_ = callback;

  PPB_AudioEncodeParameters parameters;
  parameters.channels = channels;
  parameters.input_sample_rate = input_sample_rate;
  parameters.input_sample_size = input_sample_size;
  parameters.output_profile = output_profile;
  parameters.initial_bitrate = initial_bitrate;
  parameters.acceleration = acceleration;

  Call<PpapiPluginMsg_AudioEncoder_InitializeReply>(
      RENDERER, PpapiHostMsg_AudioEncoder_Initialize(parameters),
      base::BindOnce(&AudioEncoderResource::OnPluginMsgInitializeReply, this));
  return PP_OK_COMPLETIONPENDING;
}

int32_t AudioEncoderResource::GetNumberOfSamples() {
  if (encoder_last_error_)
    return encoder_last_error_;
  return number_of_samples_;
}

int32_t AudioEncoderResource::GetBuffer(
    PP_Resource* audio_buffer,
    const scoped_refptr<TrackedCallback>& callback) {
  if (encoder_last_error_)
    return encoder_last_error_;
  if (TrackedCallback::IsPending(get_buffer_callback_))
    return PP_ERROR_INPROGRESS;

  get_buffer_data_ = audio_buffer;
  get_buffer_callback_ = callback;

  TryGetAudioBuffer();

  return PP_OK_COMPLETIONPENDING;
}

int32_t AudioEncoderResource::Encode(
    PP_Resource audio_buffer,
    const scoped_refptr<TrackedCallback>& callback) {
  if (encoder_last_error_)
    return encoder_last_error_;

  AudioBufferMap::iterator it = audio_buffers_.find(audio_buffer);
  if (it == audio_buffers_.end())
    // TODO(llandwerlin): accept MediaStreamAudioTrack's audio buffers.
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<AudioBufferResource> buffer_resource = it->second;

  encode_callbacks_.insert(
      std::make_pair(buffer_resource->GetBufferIndex(), callback));

  Post(RENDERER,
       PpapiHostMsg_AudioEncoder_Encode(buffer_resource->GetBufferIndex()));

  // Invalidate the buffer to prevent a CHECK failure when the
  // AudioBufferResource is destructed.
  buffer_resource->Invalidate();
  audio_buffers_.erase(it);

  return PP_OK_COMPLETIONPENDING;
}

int32_t AudioEncoderResource::GetBitstreamBuffer(
    PP_AudioBitstreamBuffer* bitstream_buffer,
    const scoped_refptr<TrackedCallback>& callback) {
  if (encoder_last_error_)
    return encoder_last_error_;
  if (TrackedCallback::IsPending(get_bitstream_buffer_callback_))
    return PP_ERROR_INPROGRESS;

  get_bitstream_buffer_callback_ = callback;
  get_bitstream_buffer_data_ = bitstream_buffer;

  TryWriteBitstreamBuffer();

  return PP_OK_COMPLETIONPENDING;
}

void AudioEncoderResource::RecycleBitstreamBuffer(
    const PP_AudioBitstreamBuffer* bitstream_buffer) {
  if (encoder_last_error_)
    return;

  BufferMap::const_iterator it =
      bitstream_buffer_map_.find(bitstream_buffer->buffer);
  if (it != bitstream_buffer_map_.end())
    Post(RENDERER,
         PpapiHostMsg_AudioEncoder_RecycleBitstreamBuffer(it->second));
}

void AudioEncoderResource::RequestBitrateChange(uint32_t bitrate) {
  if (encoder_last_error_)
    return;
  Post(RENDERER, PpapiHostMsg_AudioEncoder_RequestBitrateChange(bitrate));
}

void AudioEncoderResource::Close() {
  if (encoder_last_error_)
    return;
  Post(RENDERER, PpapiHostMsg_AudioEncoder_Close());
  if (!encoder_last_error_ || !initialized_)
    NotifyError(PP_ERROR_ABORTED);
  ReleaseBuffers();
}

void AudioEncoderResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(AudioEncoderResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_AudioEncoder_BitstreamBufferReady,
        OnPluginMsgBitstreamBufferReady)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(PpapiPluginMsg_AudioEncoder_EncodeReply,
                                        OnPluginMsgEncodeReply)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(PpapiPluginMsg_AudioEncoder_NotifyError,
                                        OnPluginMsgNotifyError)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  PPAPI_END_MESSAGE_MAP()
}

void AudioEncoderResource::OnPluginMsgGetSupportedProfilesReply(
    const PP_ArrayOutput& output,
    const ResourceMessageReplyParams& params,
    const std::vector<PP_AudioProfileDescription>& profiles) {
  ArrayWriter writer(output);
  if (params.result() != PP_OK || !writer.is_valid() ||
      !writer.StoreVector(profiles)) {
    SafeRunCallback(&get_supported_profiles_callback_, PP_ERROR_FAILED);
    return;
  }

  SafeRunCallback(&get_supported_profiles_callback_,
                  base::checked_cast<int32_t>(profiles.size()));
}

void AudioEncoderResource::OnPluginMsgInitializeReply(
    const ResourceMessageReplyParams& params,
    int32_t number_of_samples,
    int32_t audio_buffer_count,
    int32_t audio_buffer_size,
    int32_t bitstream_buffer_count,
    int32_t bitstream_buffer_size) {
  DCHECK(!initialized_);

  int32_t error = params.result();
  if (error) {
    SafeRunCallback(&initialize_callback_, error);
    return;
  }

  // Get audio buffers shared memory buffer.
  base::UnsafeSharedMemoryRegion region;
  if (!params.TakeUnsafeSharedMemoryRegionAtIndex(0, &region) ||
      !audio_buffer_manager_.SetBuffers(audio_buffer_count, audio_buffer_size,
                                        std::move(region), true)) {
    SafeRunCallback(&initialize_callback_, PP_ERROR_NOMEMORY);
    return;
  }

  // Get bitstream buffers shared memory buffer.
  if (!params.TakeUnsafeSharedMemoryRegionAtIndex(1, &region) ||
      !bitstream_buffer_manager_.SetBuffers(bitstream_buffer_count,
                                            bitstream_buffer_size,
                                            std::move(region), false)) {
    SafeRunCallback(&initialize_callback_, PP_ERROR_NOMEMORY);
    return;
  }

  for (int32_t i = 0; i < bitstream_buffer_manager_.number_of_buffers(); i++)
    bitstream_buffer_map_.insert(std::make_pair(
        bitstream_buffer_manager_.GetBufferPointer(i)->bitstream.data, i));

  encoder_last_error_ = PP_OK;
  number_of_samples_ = number_of_samples;
  initialized_ = true;

  SafeRunCallback(&initialize_callback_, PP_OK);
}

void AudioEncoderResource::OnPluginMsgEncodeReply(
    const ResourceMessageReplyParams& params,
    int32_t buffer_id) {
  // We need to ensure there are still callbacks to be called before
  // processing this message. We might receive an EncodeReply message after
  // having sent a Close message to the renderer. In this case, we don't
  // have any callback left to call.
  if (encode_callbacks_.empty())
    return;

  EncodeMap::iterator it = encode_callbacks_.find(buffer_id);
  CHECK(encode_callbacks_.end() != it, base::NotFatalUntil::M130);

  scoped_refptr<TrackedCallback> callback = it->second;
  encode_callbacks_.erase(it);
  SafeRunCallback(&callback, encoder_last_error_);

  audio_buffer_manager_.EnqueueBuffer(buffer_id);
  // If the plugin is waiting for an audio buffer, we can give the one
  // that just became available again.
  if (TrackedCallback::IsPending(get_buffer_callback_))
    TryGetAudioBuffer();
}

void AudioEncoderResource::OnPluginMsgBitstreamBufferReady(
    const ResourceMessageReplyParams& params,
    int32_t buffer_id) {
  bitstream_buffer_manager_.EnqueueBuffer(buffer_id);

  if (TrackedCallback::IsPending(get_bitstream_buffer_callback_))
    TryWriteBitstreamBuffer();
}

void AudioEncoderResource::OnPluginMsgNotifyError(
    const ResourceMessageReplyParams& params,
    int32_t error) {
  NotifyError(error);
}

void AudioEncoderResource::NotifyError(int32_t error) {
  DCHECK(error);

  encoder_last_error_ = error;
  SafeRunCallback(&get_supported_profiles_callback_, error);
  SafeRunCallback(&initialize_callback_, error);
  SafeRunCallback(&get_buffer_callback_, error);
  get_buffer_data_ = nullptr;
  SafeRunCallback(&get_bitstream_buffer_callback_, error);
  get_bitstream_buffer_data_ = nullptr;
  for (EncodeMap::iterator it = encode_callbacks_.begin();
       it != encode_callbacks_.end(); ++it)
    SafeRunCallback(&it->second, error);
  encode_callbacks_.clear();
}

void AudioEncoderResource::TryGetAudioBuffer() {
  DCHECK(TrackedCallback::IsPending(get_buffer_callback_));

  if (!audio_buffer_manager_.HasAvailableBuffer())
    return;

  int32_t buffer_id = audio_buffer_manager_.DequeueBuffer();
  scoped_refptr<AudioBufferResource> resource = new AudioBufferResource(
      pp_instance(), buffer_id,
      audio_buffer_manager_.GetBufferPointer(buffer_id));
  audio_buffers_.insert(
      AudioBufferMap::value_type(resource->pp_resource(), resource));

  // Take a reference for the plugin.
  *get_buffer_data_ = resource->GetReference();
  get_buffer_data_ = nullptr;
  SafeRunCallback(&get_buffer_callback_, PP_OK);
}

void AudioEncoderResource::TryWriteBitstreamBuffer() {
  DCHECK(TrackedCallback::IsPending(get_bitstream_buffer_callback_));

  if (!bitstream_buffer_manager_.HasAvailableBuffer())
    return;

  int32_t buffer_id = bitstream_buffer_manager_.DequeueBuffer();
  MediaStreamBuffer* buffer =
      bitstream_buffer_manager_.GetBufferPointer(buffer_id);

  get_bitstream_buffer_data_->buffer = buffer->bitstream.data;
  get_bitstream_buffer_data_->size = buffer->bitstream.data_size;
  get_bitstream_buffer_data_ = nullptr;
  SafeRunCallback(&get_bitstream_buffer_callback_, PP_OK);
}

void AudioEncoderResource::ReleaseBuffers() {
  for (AudioBufferMap::iterator it = audio_buffers_.begin();
       it != audio_buffers_.end(); ++it)
    it->second->Invalidate();
  audio_buffers_.clear();
}

}  // namespace proxy
}  // namespace ppapi
