// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/plugin_startup.h"

#include "base/bind.h"
#include "base/file_descriptor_posix.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/nacl_irt/manifest_service.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"

namespace ppapi {
namespace {

IPC::ChannelHandle* g_nacl_browser_ipc_handle = nullptr;
IPC::ChannelHandle* g_nacl_renderer_ipc_handle = nullptr;
IPC::ChannelHandle* g_manifest_service_handle = nullptr;

base::WaitableEvent* g_shutdown_event = NULL;
base::Thread* g_io_thread = NULL;
ManifestService* g_manifest_service = NULL;

bool IsValidChannelHandle(IPC::ChannelHandle* handle) {
  // In SFI mode the underlying handle is wrapped by a NaClIPCAdapter, which is
  // exposed as an FD. Otherwise, the handle is the underlying mojo message
  // pipe.
  return handle &&
#if defined(OS_NACL_SFI)
         handle->socket.fd != -1;
#else
         handle->is_mojo_channel_handle();
#endif
}

// Creates the manifest service on IO thread so that its Listener's thread and
// IO thread are shared. Upon completion of the manifest service creation,
// event is signaled.
void StartUpManifestServiceOnIOThread(base::WaitableEvent* event) {
  // The start up must be called only once.
  DCHECK(!g_manifest_service);
  // manifest_service_handle must be set.
  DCHECK(IsValidChannelHandle(g_manifest_service_handle));
  // IOThread and shutdown event must be initialized in advance.
  DCHECK(g_io_thread);
  DCHECK(g_shutdown_event);

  g_manifest_service = new ManifestService(
      *g_manifest_service_handle, g_io_thread->task_runner(),
      g_shutdown_event);
  event->Signal();
}

}  // namespace

void SetIPCChannelHandles(
    IPC::ChannelHandle browser_ipc_handle,
    IPC::ChannelHandle renderer_ipc_handle,
    IPC::ChannelHandle manifest_service_handle) {
  // The initialization must be only once.
  DCHECK(!g_nacl_browser_ipc_handle);
  DCHECK(!g_nacl_renderer_ipc_handle);
  DCHECK(!g_nacl_renderer_ipc_handle);
  g_nacl_browser_ipc_handle = new IPC::ChannelHandle(browser_ipc_handle);
  g_nacl_renderer_ipc_handle = new IPC::ChannelHandle(renderer_ipc_handle);
  g_manifest_service_handle = new IPC::ChannelHandle(manifest_service_handle);
}

void StartUpPlugin() {
  // The start up must be called only once.
  DCHECK(!g_shutdown_event);
  DCHECK(!g_io_thread);

  g_shutdown_event =
      new base::WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_io_thread = new base::Thread("Chrome_NaClIOThread");
  g_io_thread->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  if (IsValidChannelHandle(g_manifest_service_handle)) {
    // Manifest service must be created on IOThread so that the main message
    // handling will be done on the thread, which has a message loop
    // even before irt_ppapi_start invocation.
    // TODO(hidehiko,dmichael): This works, but is probably not well designed
    // usage. Once a better approach is made, replace this by that way.
    // (crbug.com/364241).
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    g_io_thread->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(StartUpManifestServiceOnIOThread, &event));
    event.Wait();
  }

  PPB_Audio_Shared::SetNaClMode();
}

IPC::ChannelHandle GetBrowserIPCChannelHandle() {
  // The handle must be initialized in advance.
  DCHECK(IsValidChannelHandle(g_nacl_browser_ipc_handle));
  return *g_nacl_browser_ipc_handle;
}

IPC::ChannelHandle GetRendererIPCChannelHandle() {
  // The handle must be initialized in advance.
  DCHECK(IsValidChannelHandle(g_nacl_renderer_ipc_handle));
  return *g_nacl_renderer_ipc_handle;
}

base::WaitableEvent* GetShutdownEvent() {
  // The shutdown event must be initialized in advance.
  DCHECK(g_shutdown_event);
  return g_shutdown_event;
}

base::Thread* GetIOThread() {
  // The IOThread must be initialized in advance.
  DCHECK(g_io_thread);
  return g_io_thread;
}

ManifestService* GetManifestService() {
  return g_manifest_service;
}

}  // namespace ppapi
