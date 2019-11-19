// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_input_resource.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"

namespace ppapi {
namespace proxy {

AudioInputResource::AudioInputResource(Connection connection,
                                       PP_Instance instance)
    : PluginResource(connection, instance),
      open_state_(BEFORE_OPEN),
      capturing_(false),
      shared_memory_size_(0),
      audio_input_callback_0_3_(NULL),
      audio_input_callback_(NULL),
      user_data_(NULL),
      enumeration_helper_(this),
      bytes_per_second_(0),
      sample_frame_count_(0),
      client_buffer_size_bytes_(0) {
  SendCreate(RENDERER, PpapiHostMsg_AudioInput_Create());
}

AudioInputResource::~AudioInputResource() {
  Close();
}

thunk::PPB_AudioInput_API* AudioInputResource::AsPPB_AudioInput_API() {
  return this;
}

void AudioInputResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  if (!enumeration_helper_.HandleReply(params, msg))
    PluginResource::OnReplyReceived(params, msg);
}

int32_t AudioInputResource::EnumerateDevices(
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  return enumeration_helper_.EnumerateDevices(output, callback);
}

int32_t AudioInputResource::MonitorDeviceChange(
    PP_MonitorDeviceChangeCallback callback,
    void* user_data) {
  return enumeration_helper_.MonitorDeviceChange(callback, user_data);
}

int32_t AudioInputResource::Open0_3(
    PP_Resource device_ref,
    PP_Resource config,
    PPB_AudioInput_Callback_0_3 audio_input_callback_0_3,
    void* user_data,
    scoped_refptr<TrackedCallback> callback) {
  return CommonOpen(device_ref, config, audio_input_callback_0_3, NULL,
                    user_data, callback);
}

int32_t AudioInputResource::Open(PP_Resource device_ref,
                                 PP_Resource config,
                                 PPB_AudioInput_Callback audio_input_callback,
                                 void* user_data,
                                 scoped_refptr<TrackedCallback> callback) {
  return CommonOpen(device_ref, config, NULL, audio_input_callback, user_data,
                    callback);
}

PP_Resource AudioInputResource::GetCurrentConfig() {
  // AddRef for the caller.
  if (config_.get())
    PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool AudioInputResource::StartCapture() {
  if (open_state_ == CLOSED || (open_state_ == BEFORE_OPEN &&
                                !TrackedCallback::IsPending(open_callback_))) {
    return PP_FALSE;
  }
  if (capturing_)
    return PP_TRUE;

  capturing_ = true;
  // Return directly if the audio input device hasn't been opened. Capturing
  // will be started once the open operation is completed.
  if (open_state_ == BEFORE_OPEN)
    return PP_TRUE;

  StartThread();

  Post(RENDERER, PpapiHostMsg_AudioInput_StartOrStop(true));
  return PP_TRUE;
}

PP_Bool AudioInputResource::StopCapture() {
  if (open_state_ == CLOSED)
    return PP_FALSE;
  if (!capturing_)
    return PP_TRUE;

  // If the audio input device hasn't been opened, set |capturing_| to false and
  // return directly.
  if (open_state_ == BEFORE_OPEN) {
    capturing_ = false;
    return PP_TRUE;
  }

  Post(RENDERER, PpapiHostMsg_AudioInput_StartOrStop(false));

  StopThread();
  capturing_ = false;

  return PP_TRUE;
}

void AudioInputResource::Close() {
  if (open_state_ == CLOSED)
    return;

  open_state_ = CLOSED;
  Post(RENDERER, PpapiHostMsg_AudioInput_Close());
  StopThread();

  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->PostAbort();
}

void AudioInputResource::LastPluginRefWasDeleted() {
  enumeration_helper_.LastPluginRefWasDeleted();
}

void AudioInputResource::OnPluginMsgOpenReply(
    const ResourceMessageReplyParams& params) {
  if (open_state_ == BEFORE_OPEN && params.result() == PP_OK) {
    IPC::PlatformFileForTransit socket_handle_for_transit =
        IPC::InvalidPlatformFileForTransit();
    params.TakeSocketHandleAtIndex(0, &socket_handle_for_transit);
    base::SyncSocket::Handle socket_handle =
        IPC::PlatformFileForTransitToPlatformFile(socket_handle_for_transit);
    CHECK(socket_handle != base::SyncSocket::kInvalidHandle);

    SerializedHandle serialized_shared_memory_handle =
        params.TakeHandleOfTypeAtIndex(1,
                                       SerializedHandle::SHARED_MEMORY_REGION);
    CHECK(serialized_shared_memory_handle.IsHandleValid());

    open_state_ = OPENED;
    SetStreamInfo(base::ReadOnlySharedMemoryRegion::Deserialize(
                      serialized_shared_memory_handle.TakeSharedMemoryRegion()),
                  socket_handle);
  } else {
    capturing_ = false;
  }

  // The callback may have been aborted by Close().
  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->Run(params.result());
}

void AudioInputResource::SetStreamInfo(
    base::ReadOnlySharedMemoryRegion shared_memory_region,
    base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::CancelableSyncSocket(socket_handle));
  DCHECK(!shared_memory_mapping_.IsValid());

  // Ensure that the allocated memory is enough for the audio bus and buffer
  // parameters. Note that there might be slightly more allocated memory as
  // some shared memory implementations round up to the closest 2^n when
  // allocating.
  // Example: DCHECK_GE(8208, 8192 + 16) for |sample_frame_count_| = 2048.
  shared_memory_size_ = media::ComputeAudioInputBufferSize(
      kAudioInputChannels, sample_frame_count_, 1u);
  DCHECK_GE(shared_memory_region.GetSize(), shared_memory_size_);

  // If we fail to map the shared memory into the caller's address space we
  // might as well fail here since nothing will work if this is the case.
  shared_memory_mapping_ = shared_memory_region.MapAt(0, shared_memory_size_);
  CHECK(shared_memory_mapping_.IsValid());

  // Create a new audio bus and wrap the audio data section in shared memory.
  const media::AudioInputBuffer* buffer =
      static_cast<const media::AudioInputBuffer*>(
          shared_memory_mapping_.memory());
  audio_bus_ = media::AudioBus::WrapReadOnlyMemory(
      kAudioInputChannels, sample_frame_count_, buffer->audio);

  // Create an extra integer audio buffer for user audio data callbacks.
  // Data in shared memory will be copied to this buffer, after interleaving
  // and truncation, before each input callback to match the format expected
  // by the client.
  client_buffer_size_bytes_ = audio_bus_->frames() * audio_bus_->channels() *
      kBitsPerAudioInputSample / 8;
  client_buffer_.reset(new uint8_t[client_buffer_size_bytes_]);

  // There is a pending capture request before SetStreamInfo().
  if (capturing_) {
    // Set |capturing_| to false so that the state looks consistent to
    // StartCapture(), which will reset it to true.
    capturing_ = false;
    StartCapture();
  }
}

void AudioInputResource::StartThread() {
  // Don't start the thread unless all our state is set up correctly.
  if ((!audio_input_callback_0_3_ && !audio_input_callback_) ||
      !socket_.get() || !capturing_ || !shared_memory_mapping_.memory() ||
      !audio_bus_.get() || !client_buffer_.get()) {
    return;
  }
  DCHECK(!audio_input_thread_.get());
  audio_input_thread_.reset(new base::DelegateSimpleThread(
      this, "plugin_audio_input_thread"));
  audio_input_thread_->Start();
}

void AudioInputResource::StopThread() {
  // Shut down the socket to escape any hanging |Receive|s.
  if (socket_.get())
    socket_->Shutdown();
  if (audio_input_thread_.get()) {
    audio_input_thread_->Join();
    audio_input_thread_.reset();
  }
}

void AudioInputResource::Run() {
  // The shared memory represents AudioInputBufferParameters and the actual data
  // buffer stored as an audio bus.
  const media::AudioInputBuffer* buffer =
      static_cast<const media::AudioInputBuffer*>(
          shared_memory_mapping_.memory());
  const uint32_t audio_bus_size_bytes =
      base::checked_cast<uint32_t>(shared_memory_size_ -
                                   sizeof(media::AudioInputBufferParameters));

  // This is a constantly increasing counter that is used to verify on the
  // browser side that buffers are in sync.
  uint32_t buffer_index = 0;

  while (true) {
    int pending_data = 0;
    size_t bytes_read = socket_->Receive(&pending_data, sizeof(pending_data));
    if (bytes_read != sizeof(pending_data)) {
      DCHECK_EQ(bytes_read, 0U);
      break;
    }
    if (pending_data < 0)
      break;

    // Convert an AudioBus from deinterleaved float to interleaved integer data.
    // Store the result in a preallocated |client_buffer_|.
    static_assert(kBitsPerAudioInputSample == 16,
                  "ToInterleaved expects 2 bytes.");
    audio_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
        audio_bus_->frames(), reinterpret_cast<int16_t*>(client_buffer_.get()));

    // Inform other side that we have read the data from the shared memory.
    ++buffer_index;
    size_t bytes_sent = socket_->Send(&buffer_index, sizeof(buffer_index));
    if (bytes_sent != sizeof(buffer_index)) {
      DCHECK_EQ(bytes_sent, 0U);
      break;
    }

    // While closing the stream, we may receive buffers whose size is different
    // from |data_buffer_size|.
    CHECK_LE(buffer->params.size, audio_bus_size_bytes);
    if (buffer->params.size > 0) {
      if (audio_input_callback_) {
        PP_TimeDelta latency =
            static_cast<double>(pending_data) / bytes_per_second_;
        audio_input_callback_(client_buffer_.get(),
                              client_buffer_size_bytes_,
                              latency,
                              user_data_);
      } else {
        audio_input_callback_0_3_(
            client_buffer_.get(), client_buffer_size_bytes_, user_data_);
      }
    }
  }
}

int32_t AudioInputResource::CommonOpen(
    PP_Resource device_ref,
    PP_Resource config,
    PPB_AudioInput_Callback_0_3 audio_input_callback_0_3,
    PPB_AudioInput_Callback audio_input_callback,
    void* user_data,
    scoped_refptr<TrackedCallback> callback) {
  std::string device_id;
  // |device_id| remains empty if |device_ref| is 0, which means the default
  // device.
  if (device_ref != 0) {
    thunk::EnterResourceNoLock<thunk::PPB_DeviceRef_API> enter_device_ref(
        device_ref, true);
    if (enter_device_ref.failed())
      return PP_ERROR_BADRESOURCE;
    device_id = enter_device_ref.object()->GetDeviceRefData().id;
  }

  if (TrackedCallback::IsPending(open_callback_))
    return PP_ERROR_INPROGRESS;
  if (open_state_ != BEFORE_OPEN)
    return PP_ERROR_FAILED;

  if (!audio_input_callback_0_3 && !audio_input_callback)
    return PP_ERROR_BADARGUMENT;
  thunk::EnterResourceNoLock<thunk::PPB_AudioConfig_API> enter_config(config,
                                                                      true);
  if (enter_config.failed())
    return PP_ERROR_BADARGUMENT;

  config_ = config;
  audio_input_callback_0_3_ = audio_input_callback_0_3;
  audio_input_callback_ = audio_input_callback;
  user_data_ = user_data;
  open_callback_ = callback;
  bytes_per_second_ = kAudioInputChannels * (kBitsPerAudioInputSample / 8) *
                      enter_config.object()->GetSampleRate();
  sample_frame_count_ = enter_config.object()->GetSampleFrameCount();

  PpapiHostMsg_AudioInput_Open msg(
      device_id, enter_config.object()->GetSampleRate(),
      enter_config.object()->GetSampleFrameCount());
  Call<PpapiPluginMsg_AudioInput_OpenReply>(
      RENDERER, msg,
      base::Bind(&AudioInputResource::OnPluginMsgOpenReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}
}  // namespace proxy
}  // namespace ppapi
