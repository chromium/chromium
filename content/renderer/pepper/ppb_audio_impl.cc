// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_audio_impl.h"

#include "base/check.h"
#include "content/renderer/pepper/pepper_audio_controller.h"
#include "content/renderer/pepper/pepper_platform_audio_output.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"
#include "ppapi/thunk/thunk.h"
#include "third_party/blink/public/web/web_local_frame.h"

using ppapi::PpapiGlobals;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Audio_API;
using ppapi::thunk::PPB_AudioConfig_API;
using ppapi::TrackedCallback;

namespace content {

// PPB_Audio_Impl --------------------------------------------------------------

PPB_Audio_Impl::PPB_Audio_Impl(PP_Instance instance)
    : Resource(ppapi::OBJECT_IS_IMPL, instance), audio_(nullptr) {}

PPB_Audio_Impl::~PPB_Audio_Impl() {
  PepperPluginInstanceImpl* instance = static_cast<PepperPluginInstanceImpl*>(
      PepperPluginInstance::Get(pp_instance()));
  if (instance) {
    instance->audio_controller().RemoveInstance(this);
  }

  // Calling ShutDown() makes sure StreamCreated cannot be called anymore and
  // releases the audio data associated with the pointer. Note however, that
  // until ShutDown returns, StreamCreated may still be called. This will be
  // OK since we'll just immediately clean up the data it stored later in this
  // destructor.
  if (audio_) {
    audio_->ShutDown();
    audio_ = nullptr;
  }
}

PPB_Audio_API* PPB_Audio_Impl::AsPPB_Audio_API() { return this; }

PP_Resource PPB_Audio_Impl::GetCurrentConfig() {
  // AddRef on behalf of caller, while keeping a ref for ourselves.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool PPB_Audio_Impl::StartPlayback() {
  if (!audio_)
    return PP_FALSE;
  if (playing())
    return PP_TRUE;

  // If plugin is in power saver mode, defer audio IPC communication.
  PepperPluginInstanceImpl* instance = static_cast<PepperPluginInstanceImpl*>(
      PepperPluginInstance::Get(pp_instance()));

  if (instance)
    instance->audio_controller().AddInstance(this);

  SetStartPlaybackState();
  return PP_FromBool(audio_->StartPlayback());
}

PP_Bool PPB_Audio_Impl::StopPlayback() {
  if (!audio_)
    return PP_FALSE;

  PepperPluginInstanceImpl* instance = static_cast<PepperPluginInstanceImpl*>(
      PepperPluginInstance::Get(pp_instance()));
  if (instance)
    instance->audio_controller().RemoveInstance(this);

  if (!playing())
    return PP_TRUE;
  if (!audio_->StopPlayback())
    return PP_FALSE;
  SetStopPlaybackState();

  return PP_TRUE;
}

int32_t PPB_Audio_Impl::Open(PP_Resource config,
                             scoped_refptr<TrackedCallback> create_callback) {
  // Validate the config and keep a reference to it.
  EnterResourceNoLock<PPB_AudioConfig_API> enter(config, true);
  if (enter.failed())
    return PP_ERROR_FAILED;
  config_ = config;

  PepperPluginInstanceImpl* instance = static_cast<PepperPluginInstanceImpl*>(
      PepperPluginInstance::Get(pp_instance()));
  if (!instance)
    return PP_ERROR_FAILED;

  // When the stream is created, we'll get called back on StreamCreated().
  DCHECK(!audio_);
  audio_ = PepperPlatformAudioOutput::Create(
      static_cast<int>(enter.object()->GetSampleRate()),
      static_cast<int>(enter.object()->GetSampleFrameCount()),
      instance->render_frame()->GetWebFrame()->GetLocalFrameToken(), this);
  if (!audio_)
    return PP_ERROR_FAILED;

  // At this point, we are guaranteeing ownership of the completion
  // callback.  Audio promises to fire the completion callback
  // once and only once.
  SetCreateCallback(create_callback);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_Audio_Impl::GetSyncSocket(int* sync_socket) {
  return GetSyncSocketImpl(sync_socket);
}

int32_t PPB_Audio_Impl::GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) {
  return GetSharedMemoryImpl(shm);
}

void PPB_Audio_Impl::OnSetStreamInfo(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::ScopedHandle socket_handle) {
  EnterResourceNoLock<PPB_AudioConfig_API> enter(config_, true);
  SetStreamInfo(pp_instance(), std::move(shared_memory_region),
                std::move(socket_handle), enter.object()->GetSampleRate(),
                enter.object()->GetSampleFrameCount());
}

void PPB_Audio_Impl::SetVolume(double volume) {
  if (audio_)
    audio_->SetVolume(volume);
}
}  // namespace content
