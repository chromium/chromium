// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_audio_proxy.h"

#include "base/compiler_specific.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/platform_file.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::IntToPlatformFile;
using ppapi::proxy::SerializedHandle;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Audio_API;
using ppapi::thunk::PPB_AudioConfig_API;

namespace ppapi {
namespace proxy {

class Audio : public Resource, public PPB_Audio_Shared {
 public:
  Audio(const HostResource& audio_id,
        PP_Resource config_id,
        const AudioCallbackCombined& callback,
        void* user_data);

  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  ~Audio() override;

  // Resource overrides.
  PPB_Audio_API* AsPPB_Audio_API() override;

  // PPB_Audio_API implementation.
  PP_Resource GetCurrentConfig() override;
  PP_Bool StartPlayback() override;
  PP_Bool StopPlayback() override;
  int32_t Open(PP_Resource config_id,
               scoped_refptr<TrackedCallback> create_callback) override;
  int32_t GetSyncSocket(int* sync_socket) override;
  int32_t GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) override;

 private:
  // Owning reference to the current config object. This isn't actually used,
  // we just dish it out as requested by the plugin.
  PP_Resource config_;
};

Audio::Audio(const HostResource& audio_id,
             PP_Resource config_id,
             const AudioCallbackCombined& callback,
             void* user_data)
    : Resource(OBJECT_IS_PROXY, audio_id),
      config_(config_id) {
  SetCallback(callback, user_data);
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
}

Audio::~Audio() {
#if BUILDFLAG(IS_NACL)
  // Invoke StopPlayback() to ensure audio back-end has a chance to send the
  // escape value over the sync socket, which will terminate the client side
  // audio callback loop.  This is required for NaCl Plugins that can't escape
  // by shutting down the sync_socket.
  StopPlayback();
#endif
  PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(config_);
}

PPB_Audio_API* Audio::AsPPB_Audio_API() {
  return this;
}

PP_Resource Audio::GetCurrentConfig() {
  // AddRef for the caller.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool Audio::StartPlayback() {
  if (playing())
    return PP_TRUE;
  if (!PPB_Audio_Shared::IsThreadFunctionReady())
    return PP_FALSE;
  SetStartPlaybackState();
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBAudio_StartOrStop(
          API_ID_PPB_AUDIO, host_resource(), true));
  return PP_TRUE;
}

PP_Bool Audio::StopPlayback() {
  if (!playing())
    return PP_TRUE;
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBAudio_StartOrStop(
          API_ID_PPB_AUDIO, host_resource(), false));
  SetStopPlaybackState();
  return PP_TRUE;
}

int32_t Audio::Open(PP_Resource config_id,
                    scoped_refptr<TrackedCallback> create_callback) {
  return PP_ERROR_NOTSUPPORTED;  // Don't proxy the trusted interface.
}

int32_t Audio::GetSyncSocket(int* sync_socket) {
  return PP_ERROR_NOTSUPPORTED;  // Don't proxy the trusted interface.
}

int32_t Audio::GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) {
  return PP_ERROR_NOTSUPPORTED;  // Don't proxy the trusted interface.
}

PPB_Audio_Proxy::PPB_Audio_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(this) {
}

PPB_Audio_Proxy::~PPB_Audio_Proxy() {
}

// static
PP_Resource PPB_Audio_Proxy::CreateProxyResource(
    PP_Instance instance_id,
    PP_Resource config_id,
    const AudioCallbackCombined& audio_callback,
    void* user_data) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return 0;

  EnterResourceNoLock<PPB_AudioConfig_API> config(config_id, true);
  if (config.failed())
    return 0;

  if (!audio_callback.IsValid())
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBAudio_Create(
      API_ID_PPB_AUDIO, instance_id,
      config.object()->GetSampleRate(), config.object()->GetSampleFrameCount(),
      &result));
  if (result.is_null())
    return 0;

  return (new Audio(result, config_id,
                    audio_callback, user_data))->GetReference();
}

bool PPB_Audio_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Audio_Proxy, msg)
// Don't build host side into NaCl IRT.
#if !BUILDFLAG(IS_NACL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudio_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudio_StartOrStop,
                        OnMsgStartOrStop)
#endif
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBAudio_NotifyAudioStreamCreated,
                        OnMsgNotifyAudioStreamCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if !BUILDFLAG(IS_NACL)
void PPB_Audio_Proxy::OnMsgCreate(PP_Instance instance_id,
                                  int32_t sample_rate,
                                  uint32_t sample_frame_count,
                                  HostResource* result) {
  thunk::EnterResourceCreation resource_creation(instance_id);
  if (resource_creation.failed())
    return;

  // Make the resource and get the API pointer to its trusted interface.
  result->SetHostResource(
      instance_id,
      resource_creation.functions()->CreateAudioTrusted(instance_id));
  if (result->is_null())
    return;

  // At this point, we've set the result resource, and this is a sync request.
  // Anything below this point must issue the AudioChannelConnected callback
  // to the browser. Since that's an async message, it will be issued back to
  // the plugin after the Create function returns (which is good because it
  // would be weird to get a connected message with a failure code for a
  // resource you haven't finished creating yet).
  //
  // The ...ForceCallback class will help ensure the callback is always called.
  // All error cases must call SetResult on this class.
  EnterHostFromHostResourceForceCallback<PPB_Audio_API> enter(
      *result, callback_factory_,
      &PPB_Audio_Proxy::AudioChannelConnected, *result);
  if (enter.failed())
    return;  // When enter fails, it will internally schedule the callback.

  // Make an audio config object.
  PP_Resource audio_config_res =
      resource_creation.functions()->CreateAudioConfig(
          instance_id, static_cast<PP_AudioSampleRate>(sample_rate),
          sample_frame_count);
  if (!audio_config_res) {
    enter.SetResult(PP_ERROR_FAILED);
    return;
  }

  // Initiate opening the audio object.
  enter.SetResult(enter.object()->Open(audio_config_res,
                                       enter.callback()));

  // Clean up the temporary audio config resource we made.
  const PPB_Core* core = static_cast<const PPB_Core*>(
      dispatcher()->local_get_interface()(PPB_CORE_INTERFACE));
  core->ReleaseResource(audio_config_res);
}

void PPB_Audio_Proxy::OnMsgStartOrStop(const HostResource& audio_id,
                                       bool play) {
  EnterHostFromHostResource<PPB_Audio_API> enter(audio_id);
  if (enter.failed())
    return;
  if (play)
    enter.object()->StartPlayback();
  else
    enter.object()->StopPlayback();
}

void PPB_Audio_Proxy::AudioChannelConnected(
    int32_t result,
    const HostResource& resource) {
  IPC::PlatformFileForTransit socket_handle =
      IPC::InvalidPlatformFileForTransit();
  base::UnsafeSharedMemoryRegion shared_memory_region;

  int32_t result_code = result;
  if (result_code == PP_OK) {
    result_code = GetAudioConnectedHandles(resource, &socket_handle,
                                           &shared_memory_region);
  }

  // Send all the values, even on error. This simplifies some of our cleanup
  // code since the handles will be in the other process and could be
  // inconvenient to clean up. Our IPC code will automatically handle this for
  // us, as long as the remote side always closes the handles it receives
  // (in OnMsgNotifyAudioStreamCreated), even in the failure case.
  SerializedHandle fd_wrapper(SerializedHandle::SOCKET, socket_handle);
  SerializedHandle handle_wrapper(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_memory_region)));
  dispatcher()->Send(new PpapiMsg_PPBAudio_NotifyAudioStreamCreated(
      API_ID_PPB_AUDIO, resource, result_code, std::move(fd_wrapper),
      std::move(handle_wrapper)));
}

int32_t PPB_Audio_Proxy::GetAudioConnectedHandles(
    const HostResource& resource,
    IPC::PlatformFileForTransit* foreign_socket_handle,
    base::UnsafeSharedMemoryRegion* foreign_shared_memory_region) {
  // Get the audio interface which will give us the handles.
  EnterHostFromHostResource<PPB_Audio_API> enter(resource);
  if (enter.failed())
    return PP_ERROR_NOINTERFACE;

  // Get the socket handle for signaling.
  int32_t socket_handle;
  int32_t result = enter.object()->GetSyncSocket(&socket_handle);
  if (result != PP_OK)
    return result;

  // socket_handle doesn't belong to us: don't close it.
  *foreign_socket_handle = dispatcher()->ShareHandleWithRemote(
      IntToPlatformFile(socket_handle), false);
  if (*foreign_socket_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  // Get the shared memory for the buffer.
  base::UnsafeSharedMemoryRegion* shared_memory_region;
  result = enter.object()->GetSharedMemory(&shared_memory_region);
  if (result != PP_OK)
    return result;

  // shared_memory_region doesn't belong to us: don't close it.
  *foreign_shared_memory_region =
      dispatcher()->ShareUnsafeSharedMemoryRegionWithRemote(
          *shared_memory_region);
  if (!foreign_shared_memory_region->IsValid())
    return PP_ERROR_FAILED;

  return PP_OK;
}
#endif  // !BUILDFLAG(IS_NACL)

// Processed in the plugin (message from host).
void PPB_Audio_Proxy::OnMsgNotifyAudioStreamCreated(
    const HostResource& audio_id,
    int32_t result_code,
    SerializedHandle socket_handle,
    SerializedHandle handle) {
  CHECK(socket_handle.is_socket());
  CHECK(handle.is_shmem_region());
  EnterPluginFromHostResource<PPB_Audio_API> enter(audio_id);
  if (enter.failed() || result_code != PP_OK) {
    // The caller may still have given us these handles in the failure case.
    // The easiest way to clean socket handle up is to just put them in the
    // SyncSocket object and then close it. The shared memory region will be
    // cleaned up automatically. This failure case is not performance critical.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(socket_handle.descriptor()));
  } else {
    EnterResourceNoLock<PPB_AudioConfig_API> config(
        static_cast<Audio*>(enter.object())->GetCurrentConfig(), true);
    static_cast<Audio*>(enter.object())
        ->SetStreamInfo(enter.resource()->pp_instance(),
                        base::UnsafeSharedMemoryRegion::Deserialize(
                            handle.TakeSharedMemoryRegion()),
                        base::SyncSocket::ScopedHandle(
                            IPC::PlatformFileForTransitToPlatformFile(
                                socket_handle.descriptor())),
                        config.object()->GetSampleRate(),
                        config.object()->GetSampleFrameCount());
  }
}

}  // namespace proxy
}  // namespace ppapi
