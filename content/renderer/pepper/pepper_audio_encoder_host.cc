// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_audio_encoder_host.h"

#include <stddef.h>

#include <utility>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/bind_post_task.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "third_party/opus/src/include/opus.h"

using ppapi::proxy::SerializedHandle;

namespace content {

namespace {

// Buffer up to 150ms (15 x 10ms per frame).
const uint32_t kDefaultNumberOfAudioBuffers = 15;

bool PP_HardwareAccelerationCompatibleAudio(bool accelerated,
                                            PP_HardwareAcceleration requested) {
  switch (requested) {
    case PP_HARDWAREACCELERATION_ONLY:
      return accelerated;
    case PP_HARDWAREACCELERATION_NONE:
      return !accelerated;
    case PP_HARDWAREACCELERATION_WITHFALLBACK:
      return true;
      // No default case, to catch unhandled PP_HardwareAcceleration values.
  }
  return false;
}

}  // namespace

// This class should be constructed and initialized on the main renderer
// thread, used and destructed on the media thread.
class PepperAudioEncoderHost::AudioEncoderImpl {
 public:
  // Callback used to signal encoded data. If |size| is negative, an error
  // occurred.
  using BitstreamBufferReadyCB = base::OnceCallback<void(int32_t size)>;

  AudioEncoderImpl();

  AudioEncoderImpl(const AudioEncoderImpl&) = delete;
  AudioEncoderImpl& operator=(const AudioEncoderImpl&) = delete;

  ~AudioEncoderImpl();

  // Used on the renderer thread.
  static std::vector<PP_AudioProfileDescription> GetSupportedProfiles();
  bool Initialize(const ppapi::proxy::PPB_AudioEncodeParameters& parameters);
  int32_t GetNumberOfSamplesPerFrame();

  // Used on the media thread.
  void Encode(uint8_t* input_data,
              size_t input_size,
              uint8_t* output_data,
              size_t output_size,
              BitstreamBufferReadyCB callback);
  void RequestBitrateChange(uint32_t bitrate);

 private:
  base::HeapArray<uint8_t> encoder_memory_;
  OpusEncoder* opus_encoder_;

  // Initialization parameters, only valid if |encoder_memory_| is not
  // nullptr.
  ppapi::proxy::PPB_AudioEncodeParameters parameters_;
};

PepperAudioEncoderHost::AudioEncoderImpl::AudioEncoderImpl()
    : opus_encoder_(nullptr) {}

PepperAudioEncoderHost::AudioEncoderImpl::~AudioEncoderImpl() {}

// static
std::vector<PP_AudioProfileDescription>
PepperAudioEncoderHost::AudioEncoderImpl::GetSupportedProfiles() {
  std::vector<PP_AudioProfileDescription> profiles;
  static const uint32_t sampling_rates[] = {8000, 12000, 16000, 24000, 48000};

  for (uint32_t i = 0; i < std::size(sampling_rates); ++i) {
    PP_AudioProfileDescription profile;
    profile.profile = PP_AUDIOPROFILE_OPUS;
    profile.max_channels = 2;
    profile.sample_size = PP_AUDIOBUFFER_SAMPLESIZE_16_BITS;
    profile.sample_rate = sampling_rates[i];
    profile.hardware_accelerated = PP_FALSE;
    profiles.push_back(profile);
  }
  return profiles;
}

bool PepperAudioEncoderHost::AudioEncoderImpl::Initialize(
    const ppapi::proxy::PPB_AudioEncodeParameters& parameters) {
  if (parameters.output_profile != PP_AUDIOPROFILE_OPUS)
    return false;

  DCHECK(encoder_memory_.empty());

  int32_t encoder_size = opus_encoder_get_size(parameters.channels);
  if (encoder_size < 1)
    return false;

  auto encoder_memory = base::HeapArray<uint8_t>::Uninit(encoder_size);
  opus_encoder_ = reinterpret_cast<OpusEncoder*>(encoder_memory.data());

  if (opus_encoder_init(opus_encoder_, parameters.input_sample_rate,
                        parameters.channels, OPUS_APPLICATION_AUDIO) != OPUS_OK)
    return false;

  if (opus_encoder_ctl(opus_encoder_,
                       OPUS_SET_BITRATE(parameters.initial_bitrate <= 0
                                            ? OPUS_AUTO
                                            : parameters.initial_bitrate)) !=
      OPUS_OK)
    return false;

  encoder_memory_ = std::move(encoder_memory);
  parameters_ = parameters;

  return true;
}

int32_t PepperAudioEncoderHost::AudioEncoderImpl::GetNumberOfSamplesPerFrame() {
  DCHECK(!encoder_memory_.empty());
  // Opus supports 2.5, 5, 10, 20, 40 or 60ms audio frames. We take
  // 10ms by default.
  return parameters_.input_sample_rate / 100;
}

void PepperAudioEncoderHost::AudioEncoderImpl::Encode(
    uint8_t* input_data,
    size_t input_size,
    uint8_t* output_data,
    size_t output_size,
    BitstreamBufferReadyCB callback) {
  DCHECK(!encoder_memory_.empty());
  int32_t result = opus_encode(
      opus_encoder_, reinterpret_cast<opus_int16*>(input_data),
      (input_size / parameters_.channels) / parameters_.input_sample_size,
      output_data, output_size);
  std::move(callback).Run(result);
}

void PepperAudioEncoderHost::AudioEncoderImpl::RequestBitrateChange(
    uint32_t bitrate) {
  DCHECK(!encoder_memory_.empty());
  opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(bitrate));
}

PepperAudioEncoderHost::PepperAudioEncoderHost(RendererPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      initialized_(false),
      encoder_last_error_(PP_ERROR_FAILED),
      media_task_runner_(
          RenderThreadImpl::current()->GetMediaThreadTaskRunner()) {}

PepperAudioEncoderHost::~PepperAudioEncoderHost() {
  Close();
}

int32_t PepperAudioEncoderHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperAudioEncoderHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_AudioEncoder_GetSupportedProfiles,
        OnHostMsgGetSupportedProfiles)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_AudioEncoder_Initialize,
                                      OnHostMsgInitialize)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_AudioEncoder_Encode,
                                      OnHostMsgEncode)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_AudioEncoder_RecycleBitstreamBuffer,
        OnHostMsgRecycleBitstreamBuffer)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_AudioEncoder_RequestBitrateChange,
        OnHostMsgRequestBitrateChange)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_AudioEncoder_Close,
                                        OnHostMsgClose)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperAudioEncoderHost::OnHostMsgGetSupportedProfiles(
    ppapi::host::HostMessageContext* context) {
  std::vector<PP_AudioProfileDescription> profiles;
  GetSupportedProfiles(&profiles);

  host()->SendReply(
      context->MakeReplyMessageContext(),
      PpapiPluginMsg_AudioEncoder_GetSupportedProfilesReply(profiles));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperAudioEncoderHost::OnHostMsgInitialize(
    ppapi::host::HostMessageContext* context,
    const ppapi::proxy::PPB_AudioEncodeParameters& parameters) {
  if (initialized_)
    return PP_ERROR_FAILED;

  if (!IsInitializationValid(parameters))
    return PP_ERROR_NOTSUPPORTED;

  std::unique_ptr<AudioEncoderImpl> encoder(new AudioEncoderImpl);
  if (!encoder->Initialize(parameters))
    return PP_ERROR_FAILED;
  if (!AllocateBuffers(parameters, encoder->GetNumberOfSamplesPerFrame()))
    return PP_ERROR_NOMEMORY;

  initialized_ = true;
  encoder_last_error_ = PP_OK;
  encoder_.swap(encoder);

  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(SerializedHandle(
      renderer_ppapi_host_->ShareUnsafeSharedMemoryRegionWithRemote(
          audio_buffer_manager_->region())));
  reply_context.params.AppendHandle(SerializedHandle(
      renderer_ppapi_host_->ShareUnsafeSharedMemoryRegionWithRemote(
          bitstream_buffer_manager_->region())));
  host()->SendReply(reply_context,
                    PpapiPluginMsg_AudioEncoder_InitializeReply(
                        encoder_->GetNumberOfSamplesPerFrame(),
                        audio_buffer_manager_->number_of_buffers(),
                        audio_buffer_manager_->buffer_size(),
                        bitstream_buffer_manager_->number_of_buffers(),
                        bitstream_buffer_manager_->buffer_size()));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperAudioEncoderHost::OnHostMsgEncode(
    ppapi::host::HostMessageContext* context,
    int32_t buffer_id) {
  if (encoder_last_error_)
    return encoder_last_error_;

  if (buffer_id < 0 || buffer_id >= audio_buffer_manager_->number_of_buffers())
    return PP_ERROR_FAILED;

  audio_buffer_manager_->EnqueueBuffer(buffer_id);

  DoEncode();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperAudioEncoderHost::OnHostMsgRecycleBitstreamBuffer(
    ppapi::host::HostMessageContext* context,
    int32_t buffer_id) {
  if (encoder_last_error_)
    return encoder_last_error_;

  if (buffer_id < 0 ||
      buffer_id >= bitstream_buffer_manager_->number_of_buffers())
    return PP_ERROR_FAILED;

  bitstream_buffer_manager_->EnqueueBuffer(buffer_id);

  DoEncode();

  return PP_OK;
}

int32_t PepperAudioEncoderHost::OnHostMsgRequestBitrateChange(
    ppapi::host::HostMessageContext* context,
    uint32_t bitrate) {
  if (encoder_last_error_)
    return encoder_last_error_;

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioEncoderImpl::RequestBitrateChange,
                                base::Unretained(encoder_.get()), bitrate));

  return PP_OK;
}

int32_t PepperAudioEncoderHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context) {
  encoder_last_error_ = PP_ERROR_FAILED;
  Close();

  return PP_OK;
}

void PepperAudioEncoderHost::GetSupportedProfiles(
    std::vector<PP_AudioProfileDescription>* profiles) {
  DCHECK(RenderThreadImpl::current());

  *profiles = AudioEncoderImpl::GetSupportedProfiles();
}

bool PepperAudioEncoderHost::IsInitializationValid(
    const ppapi::proxy::PPB_AudioEncodeParameters& parameters) {
  DCHECK(RenderThreadImpl::current());

  std::vector<PP_AudioProfileDescription> profiles;
  GetSupportedProfiles(&profiles);

  for (const PP_AudioProfileDescription& profile : profiles) {
    if (parameters.output_profile == profile.profile &&
        parameters.input_sample_size == profile.sample_size &&
        parameters.input_sample_rate == profile.sample_rate &&
        parameters.channels <= profile.max_channels &&
        PP_HardwareAccelerationCompatibleAudio(
            profile.hardware_accelerated == PP_TRUE, parameters.acceleration))
      return true;
  }

  return false;
}

bool PepperAudioEncoderHost::AllocateBuffers(
    const ppapi::proxy::PPB_AudioEncodeParameters& parameters,
    int32_t samples_per_frame) {
  DCHECK(RenderThreadImpl::current());

  // Audio buffers size computation.
  base::CheckedNumeric<uint32_t> audio_buffer_size = samples_per_frame;
  audio_buffer_size *= parameters.channels;
  audio_buffer_size *= parameters.input_sample_size;

  base::CheckedNumeric<uint32_t> total_audio_buffer_size = audio_buffer_size;
  total_audio_buffer_size += sizeof(ppapi::MediaStreamBuffer::Audio);
  base::CheckedNumeric<size_t> total_audio_memory_size =
      total_audio_buffer_size;
  total_audio_memory_size *= kDefaultNumberOfAudioBuffers;

  // Bitstream buffers size computation (individual bitstream buffers are
  // twice the size of the raw data, to handle the worst case where
  // compression doesn't work).
  base::CheckedNumeric<uint32_t> bitstream_buffer_size = audio_buffer_size;
  bitstream_buffer_size *= 2;
  bitstream_buffer_size += sizeof(ppapi::MediaStreamBuffer::Bitstream);
  base::CheckedNumeric<size_t> total_bitstream_memory_size =
      bitstream_buffer_size;
  total_bitstream_memory_size *= kDefaultNumberOfAudioBuffers;

  if (!total_audio_memory_size.IsValid() ||
      !total_bitstream_memory_size.IsValid())
    return false;

  base::UnsafeSharedMemoryRegion audio_region =
      base::UnsafeSharedMemoryRegion::Create(
          total_audio_memory_size.ValueOrDie());
  if (!audio_region.IsValid())
    return false;
  std::unique_ptr<ppapi::MediaStreamBufferManager> audio_buffer_manager(
      new ppapi::MediaStreamBufferManager(this));
  if (!audio_buffer_manager->SetBuffers(
          kDefaultNumberOfAudioBuffers,
          base::ValueOrDieForType<int32_t>(total_audio_buffer_size),
          std::move(audio_region), false))
    return false;

  for (int32_t i = 0; i < audio_buffer_manager->number_of_buffers(); ++i) {
    ppapi::MediaStreamBuffer::Audio* buffer =
        &(audio_buffer_manager->GetBufferPointer(i)->audio);
    buffer->header.size = total_audio_buffer_size.ValueOrDie();
    buffer->header.type = ppapi::MediaStreamBuffer::TYPE_AUDIO;
    buffer->sample_rate =
        static_cast<PP_AudioBuffer_SampleRate>(parameters.input_sample_rate);
    buffer->number_of_channels = parameters.channels;
    buffer->number_of_samples = samples_per_frame;
    buffer->data_size = audio_buffer_size.ValueOrDie();
  }

  base::UnsafeSharedMemoryRegion bitstream_region =
      base::UnsafeSharedMemoryRegion::Create(
          total_bitstream_memory_size.ValueOrDie());
  if (!bitstream_region.IsValid())
    return false;
  std::unique_ptr<ppapi::MediaStreamBufferManager> bitstream_buffer_manager(
      new ppapi::MediaStreamBufferManager(this));
  if (!bitstream_buffer_manager->SetBuffers(
          kDefaultNumberOfAudioBuffers,
          base::ValueOrDieForType<int32_t>(bitstream_buffer_size),
          std::move(bitstream_region), true))
    return false;

  for (int32_t i = 0; i < bitstream_buffer_manager->number_of_buffers(); ++i) {
    ppapi::MediaStreamBuffer::Bitstream* buffer =
        &(bitstream_buffer_manager->GetBufferPointer(i)->bitstream);
    buffer->header.size = bitstream_buffer_size.ValueOrDie();
    buffer->header.type = ppapi::MediaStreamBuffer::TYPE_BITSTREAM;
  }

  audio_buffer_manager_.swap(audio_buffer_manager);
  bitstream_buffer_manager_.swap(bitstream_buffer_manager);

  return true;
}

void PepperAudioEncoderHost::DoEncode() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(!encoder_last_error_);

  if (!audio_buffer_manager_->HasAvailableBuffer() ||
      !bitstream_buffer_manager_->HasAvailableBuffer())
    return;

  int32_t audio_buffer_id = audio_buffer_manager_->DequeueBuffer();
  int32_t bitstream_buffer_id = bitstream_buffer_manager_->DequeueBuffer();

  ppapi::MediaStreamBuffer* audio_buffer =
      audio_buffer_manager_->GetBufferPointer(audio_buffer_id);
  ppapi::MediaStreamBuffer* bitstream_buffer =
      bitstream_buffer_manager_->GetBufferPointer(bitstream_buffer_id);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioEncoderImpl::Encode,
                     base::Unretained(encoder_.get()),
                     static_cast<uint8_t*>(audio_buffer->audio.data),
                     audio_buffer_manager_->buffer_size() -
                         sizeof(ppapi::MediaStreamBuffer::Audio),
                     static_cast<uint8_t*>(bitstream_buffer->bitstream.data),
                     bitstream_buffer_manager_->buffer_size() -
                         sizeof(ppapi::MediaStreamBuffer::Bitstream),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &PepperAudioEncoderHost::BitstreamBufferReady,
                         weak_ptr_factory_.GetWeakPtr(), audio_buffer_id,
                         bitstream_buffer_id))));
}

void PepperAudioEncoderHost::BitstreamBufferReady(int32_t audio_buffer_id,
                                                  int32_t bitstream_buffer_id,
                                                  int32_t result) {
  DCHECK(RenderThreadImpl::current());

  if (encoder_last_error_)
    return;

  if (result < 0) {
    NotifyPepperError(PP_ERROR_FAILED);
    return;
  }

  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_AudioEncoder_EncodeReply(audio_buffer_id));

  ppapi::MediaStreamBuffer::Bitstream* buffer =
      &(bitstream_buffer_manager_->GetBufferPointer(bitstream_buffer_id)
            ->bitstream);
  buffer->data_size = static_cast<uint32_t>(result);

  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_AudioEncoder_BitstreamBufferReady(bitstream_buffer_id));
}

void PepperAudioEncoderHost::NotifyPepperError(int32_t error) {
  DCHECK(RenderThreadImpl::current());

  encoder_last_error_ = error;
  Close();
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_AudioEncoder_NotifyError(encoder_last_error_));
}

void PepperAudioEncoderHost::Close() {
  DCHECK(RenderThreadImpl::current());

  // Destroy the encoder and the audio/bitstream buffers on the media thread
  // to avoid freeing memory the encoder might still be working on.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StopAudioEncoder, std::move(encoder_),
                                std::move(audio_buffer_manager_),
                                std::move(bitstream_buffer_manager_)));
}

// static
void PepperAudioEncoderHost::StopAudioEncoder(
    std::unique_ptr<AudioEncoderImpl> encoder,
    std::unique_ptr<ppapi::MediaStreamBufferManager> audio_buffer_manager,
    std::unique_ptr<ppapi::MediaStreamBufferManager> bitstream_buffer_manager) {
}

}  // namespace content
