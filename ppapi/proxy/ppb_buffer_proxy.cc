// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_buffer_proxy.h"

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

Buffer::Buffer(const HostResource& resource,
               base::UnsafeSharedMemoryRegion shm_region)
    : Resource(OBJECT_IS_PROXY, resource),
      shm_(std::move(shm_region)),
      map_count_(0) {}

Buffer::~Buffer() {
  Unmap();
}

thunk::PPB_Buffer_API* Buffer::AsPPB_Buffer_API() {
  return this;
}

PP_Bool Buffer::Describe(uint32_t* size_in_bytes) {
  *size_in_bytes = shm_.GetSize();
  return PP_TRUE;
}

PP_Bool Buffer::IsMapped() {
  return PP_FromBool(map_count_ > 0);
}

void* Buffer::Map() {
  if (map_count_++ == 0)
    mapping_ = shm_.Map();
  return mapping_.memory();
}

void Buffer::Unmap() {
  if (--map_count_ == 0)
    mapping_ = {};
}

int32_t Buffer::GetSharedMemory(base::UnsafeSharedMemoryRegion** out_handle) {
  NOTREACHED();
  return PP_ERROR_NOTSUPPORTED;
}

PPB_Buffer_Proxy::PPB_Buffer_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Buffer_Proxy::~PPB_Buffer_Proxy() {
}

// static
PP_Resource PPB_Buffer_Proxy::CreateProxyResource(PP_Instance instance,
                                                  uint32_t size) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  ppapi::proxy::SerializedHandle shm_handle;
  dispatcher->Send(new PpapiHostMsg_PPBBuffer_Create(
      API_ID_PPB_BUFFER, instance, size, &result, &shm_handle));
  if (result.is_null() || !shm_handle.IsHandleValid() ||
      !shm_handle.is_shmem_region())
    return 0;

  return AddProxyResource(result, base::UnsafeSharedMemoryRegion::Deserialize(
                                      shm_handle.TakeSharedMemoryRegion()));
}

// static
PP_Resource PPB_Buffer_Proxy::AddProxyResource(
    const HostResource& resource,
    base::UnsafeSharedMemoryRegion shm_region) {
  return (new Buffer(resource, std::move(shm_region)))->GetReference();
}

bool PPB_Buffer_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Buffer_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBBuffer_Create, OnMsgCreate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Buffer_Proxy::OnMsgCreate(
    PP_Instance instance,
    uint32_t size,
    HostResource* result_resource,
    ppapi::proxy::SerializedHandle* result_shm_handle) {
  // Overwritten below on success.
  result_shm_handle->set_null_shmem_region();
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  if (!dispatcher->permissions().HasPermission(ppapi::PERMISSION_DEV))
    return;

  thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return;
  PP_Resource local_buffer_resource = enter.functions()->CreateBuffer(instance,
                                                                      size);
  if (local_buffer_resource == 0)
    return;

  thunk::EnterResourceNoLock<thunk::PPB_Buffer_API> trusted_buffer(
      local_buffer_resource, false);
  if (trusted_buffer.failed())
    return;
  base::UnsafeSharedMemoryRegion* local_shm;
  if (trusted_buffer.object()->GetSharedMemory(&local_shm) != PP_OK)
    return;

  result_resource->SetHostResource(instance, local_buffer_resource);

  result_shm_handle->set_unsafe_shmem_region(
      dispatcher->ShareUnsafeSharedMemoryRegionWithRemote(*local_shm));
}

}  // namespace proxy
}  // namespace ppapi
