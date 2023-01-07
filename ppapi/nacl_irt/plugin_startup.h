// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_PLUGIN_STARTUP_H_
#define PPAPI_NACL_IRT_PLUGIN_STARTUP_H_

#include "ipc/ipc_channel_handle.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace base {
class Thread;
class WaitableEvent;
}  // namespace base

namespace ppapi {

class ManifestService;

// Sets the IPC channels for the browser and the renderer.
// Must be called before the ppapi_start() IRT interface is called.
PPAPI_PROXY_EXPORT void SetIPCChannelHandles(
    IPC::ChannelHandle browser_ipc_handle,
    IPC::ChannelHandle renderer_ipc_handle,
    IPC::ChannelHandle manifest_service_handle);

// Runs start up procedure for the plugin.
// Specifically, start background IO thread for IPC, and prepare
// shutdown event instance.
PPAPI_PROXY_EXPORT void StartUpPlugin();

// Returns IPC channel handle for PPAPI to the browser.
IPC::ChannelHandle GetBrowserIPCChannelHandle();

// Returns IPC channel handle for PPAPI to the renderer.
IPC::ChannelHandle GetRendererIPCChannelHandle();

// Returns the shutdown event instance for the plugin.
// Must be called after StartUpPlugin().
base::WaitableEvent* GetShutdownEvent();

// Returns the IOThread for the plugin. Must be called after StartUpPlugin().
base::Thread* GetIOThread();

// Returns the ManifestService interface. To use this, manifest_service_handle
// needs to be set via SetIPCChannelHandles. Must be called after
// StartUpPlugin().
// If not available, returns NULL.
ManifestService* GetManifestService();

}  // namespace ppapi

#endif  // PPAPI_NACL_IRT_PLUGIN_STARTUP_H_
