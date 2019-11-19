// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_output_resource.h"

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

AudioOutputResource::AudioOutputResource(Connection connection,
                                         PP_Instance instance)
    : PluginResource(connection, instance),
      open_state_(BEFORE_OPEN),
      playing_(false),
      shared_memory_size_(0),
      audio_output_callback_(NULL),
      user_data_(NULL),
      enumeration_helper_(this),
      bytes_per_second_(0),
      sample_frame_count_(0),
      client_buffer_size_bytes_(0) {
  SendCreate(RENDERER, PpapiHostMsg_AudioOutput_Create());
}

AudioOutputResource::~AudioOutputResource() {
  Close();
}

thunk::PPB_AudioOutput_API* AudioOutputResource::AsPPB_AudioOutput_API() {
  return this;
}

void AudioOutputResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  if (!enumeration_helper_.HandleReply(params, msg))
    PluginResource::OnReplyReceived(params, msg);
}

int32_t AudioOutputResource::EnumerateDevices(
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  return enumeration_helper_.EnumerateDevices(output, callback);
}

int32_t AudioOutputResource::MonitorDeviceChange(
    PP_MonitorDeviceChangeCallback callback,
    void* user_data) {
  return enumeration_helper_.MonitorDeviceChange(callback, user_data);
}

int32_t AudioOutputResource::Open(
    PP_Resource device_ref,
    PP_Resource config,
    PPB_AudioOutput_Callback audio_output_callback,
    void* user_data,
    scoped_refptr<TrackedCallback> callback) {
  return CommonOpen(device_ref, config, audio_output_callback, user_data,
                    callback);
}

PP_Resource AudioOutputResource::GetCurrentConfig() {
  // AddRef for the caller.
  if (config_.get())
    PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool AudioOutputResource::StartPlayback() {
  if (open_state_ == CLOSED || (open_state_ == BEFORE_OPEN &&
                                !TrackedCallback::IsPending(open_callback_))) {
    return PP_FALSE;
  }
  if (playing_)
    return PP_TRUE;

  playing_ = true;

  StartThread();

  Post(RENDERER, PpapiHostMsg_AudioOutput_StartOrStop(true));
  return PP_TRUE;
}

PP_Bool AudioOutputResource::StopPlayback() {
  if (open_state_ == CLOSED)
    return PP_FALSE;
  if (!playing_)
    return PP_TRUE;

  // If the audio output device hasn't been opened, set |playing_| to false and
  // return directly.
  if (open_state_ == BEFORE_OPEN) {
    playing_ = false;
    return PP_TRUE;
  }

  Post(RENDERER, PpapiHostMsg_AudioOutput_StartOrStop(false));

  StopThread();
  playing_ = false;

  return PP_TRUE;
}

void AudioOutputResource::Close() {
  if (open_state_ == CLOSED)
    return;

  open_state_ = CLOSED;
  Post(RENDERER, PpapiHostMsg_AudioOutput_Close());
  StopThread();

  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->PostAbort();
}

void AudioOutputResource::LastPluginRefWasDeleted() {
  enumeration_helper_.LastPluginRefWasDeleted();
}

void AudioOutputResource::OnPluginMsgOpenReply(
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
    SetStreamInfo(base::UnsafeSharedMemoryRegion::Deserialize(
                      serialized_shared_memory_handle.TakeSharedMemoryRegion()),
                  socket_handle);
  } else {
    playing_ = false;
  }

  // The callback may have been aborted by Close().
  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->Run(params.result());
}

void AudioOutputResource::SetStreamInfo(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::CancelableSyncSocket(socket_handle));

  // Ensure that the allocated memory is enough for the audio bus and buffer
  // parameters. Note that there might be slightly more allocated memory as
  // some shared memory implementations round up to the closest 2^n when
  // allocating.
  // Example: DCHECK_GE(8208, 8192 + 16) for |sample_frame_count_| = 2048.
  shared_memory_size_ = media::ComputeAudioOutputBufferSize(
      kAudioOutputChannels, sample_frame_count_);
  DCHECK_GE(shared_memory_region.GetSize(), shared_memory_size_);

  // If we fail to map the shared memory into the caller's address space we
  // might as well fail here since nothing will work if this is the case.
  shared_memory_mapping_ = shared_memory_region.MapAt(0, shared_memory_size_);
  CHECK(shared_memory_mapping_.IsValid());

  // Create a new audio bus and wrap the audio data section in shared memory.
  media::AudioOutputBuffer* buffer =
      static_cast<media::AudioOutputBuffer*>(shared_memory_mapping_.memory());
  audio_bus_ = media::AudioBus::WrapMemory(kAudioOutputChannels,
                                           sample_frame_count_, buffer->audio);

  // Setup integer audio buffer for user audio data
  client_buffer_size_bytes_ = audio_bus_->frames() * audio_bus_->channels() *
                              kBitsPerAudioOutputSample / 8;
  client_buffer_.reset(new uint8_t[client_buffer_size_bytes_]);
}

void AudioOutputResource::StartThread() {
  // Don't start the thread unless all our state is set up correctly.
  if (!audio_output_callback_ || !socket_.get() ||
      !shared_memory_mapping_.memory() || !audio_bus_.get() ||
      !client_buffer_.get() || bytes_per_second_ == 0)
    return;

  // Clear contents of shm buffer before starting audio thread. This will
  // prevent a burst of static if for some reason the audio thread doesn't
  // start up quickly enough.
  memset(shared_memory_mapping_.memory(), 0, shared_memory_size_);
  memset(client_buffer_.get(), 0, client_buffer_size_bytes_);

  DCHECK(!audio_output_thread_.get());
  audio_output_thread_.reset(
      new base::DelegateSimpleThread(this, "plugin_audio_output_thread"));
  audio_output_thread_->Start();
}

void AudioOutputResource::StopThread() {
  // Shut down the socket to escape any hanging |Receive|s.
  if (socket_.get())
    socket_->Shutdown();
  if (audio_output_thread_.get()) {
    audio_output_thread_->Join();
    audio_output_thread_.reset();
  }
}

void AudioOutputResource::Run() {
  // The shared memory represents AudioOutputBufferParameters and the actual
  // data buffer stored as an audio bus.
  media::AudioOutputBuffer* buffer =
      static_cast<media::AudioOutputBuffer*>(shared_memory_mapping_.memory());

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

    {
      base::TimeDelta delay =
          base::TimeDelta::FromMicroseconds(buffer->params.delay_us);

      audio_output_callback_(client_buffer_.get(), client_buffer_size_bytes_,
                             delay.InSecondsF(), user_data_);
    }

    // Deinterleave the audio data into the shared memory as floats.
    static_assert(kBitsPerAudioOutputSample == 16,
                  "FromInterleaved expects 2 bytes.");
    audio_bus_->FromInterleaved<media::SignedInt16SampleTypeTraits>(
        reinterpret_cast<int16_t*>(client_buffer_.get()), audio_bus_->frames());

    // Inform other side that we have read the data from the shared memory.
    // Let the other end know which buffer we just filled.  The buffer index is
    // used to ensure the other end is getting the buffer it expects.  For more
    // details on how this works see AudioSyncReader::WaitUntilDataIsReady().
    ++buffer_index;
    size_t bytes_sent = socket_->Send(&buffer_index, sizeof(buffer_index));
    if (bytes_sent != sizeof(buffer_index)) {
      DCHECK_EQ(bytes_sent, 0U);
      break;
    }
  }
}

int32_t AudioOutputResource::CommonOpen(
    PP_Resource device_ref,
    PP_Resource config,
    PPB_AudioOutput_Callback audio_output_callback,
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

  if (!audio_output_callback)
    return PP_ERROR_BADARGUMENT;
  thunk::EnterResourceNoLock<thunk::PPB_AudioConfig_API> enter_config(config,
                                                                      true);
  if (enter_config.failed())
    return PP_ERROR_BADARGUMENT;

  config_ = config;
  audio_output_callback_ = audio_output_callback;
  user_data_ = user_data;
  open_callback_ = callback;
  bytes_per_second_ = kAudioOutputChannels * (kBitsPerAudioOutputSample / 8) *
                      enter_config.object()->GetSampleRate();
  sample_frame_count_ = enter_config.object()->GetSampleFrameCount();

  PpapiHostMsg_AudioOutput_Open msg(
      device_id, enter_config.object()->GetSampleRate(),
      enter_config.object()->GetSampleFrameCount());
  Call<PpapiPluginMsg_AudioOutput_OpenReply>(
      RENDERER, msg,
      base::Bind(&AudioOutputResource::OnPluginMsgOpenReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}
}  // namespace proxy
}  // namespace ppapi
