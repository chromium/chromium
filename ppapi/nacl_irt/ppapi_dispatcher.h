// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_PPAPI_DISPATCHER_H_
#define PPAPI_NACL_IRT_PPAPI_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sender.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"

struct PP_BrowserFont_Trusted_Description;

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}  // namespace base

namespace IPC {
class Message;
class SyncChannel;
}  // namespace IPC

namespace ppapi {

struct PpapiNaClPluginArgs;
struct Preferences;

// This class manages communication between the plugin and the browser, and
// manages the PluginDispatcher instances for communication between the plugin
// and the renderer.
class PpapiDispatcher : public proxy::PluginDispatcher::PluginDelegate,
                        public proxy::PluginProxyDelegate,
                        public IPC::Listener,
                        public IPC::Sender {
 public:
  PpapiDispatcher(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                  base::WaitableEvent* shutdown_event,
                  IPC::ChannelHandle browser_ipc_handle,
                  IPC::ChannelHandle renderer_ipc_handle);

  // PluginDispatcher::PluginDelegate implementation.
  base::SingleThreadTaskRunner* GetIPCTaskRunner() override;
  base::WaitableEvent* GetShutdownEvent() override;
  IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId peer_pid,
      bool should_close_source) override;
  base::UnsafeSharedMemoryRegion ShareUnsafeSharedMemoryRegionWithRemote(
      const base::UnsafeSharedMemoryRegion& region,
      base::ProcessId remote_pid) override;
  base::ReadOnlySharedMemoryRegion ShareReadOnlySharedMemoryRegionWithRemote(
      const base::ReadOnlySharedMemoryRegion& region,
      base::ProcessId remote_pid) override;
  std::set<PP_Instance>* GetGloballySeenInstanceIDSet() override;
  uint32_t Register(proxy::PluginDispatcher* plugin_dispatcher) override;
  void Unregister(uint32_t plugin_dispatcher_id) override;

  // PluginProxyDelegate implementation.
  IPC::Sender* GetBrowserSender() override;
  std::string GetUILanguage() override;
  void PreCacheFontForFlash(const void* logfontw) override;
  void SetActiveURL(const std::string& url) override;
  PP_Resource CreateBrowserFont(proxy::Connection connection,
                                PP_Instance instance,
                                const PP_BrowserFont_Trusted_Description& desc,
                                const Preferences& prefs) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // IPC::Sender implementation
  bool Send(IPC::Message* message) override;

 private:
  void OnMsgInitializeNaClDispatcher(const PpapiNaClPluginArgs& args);
  void OnPluginDispatcherMessageReceived(const IPC::Message& msg);

  std::set<PP_Instance> instances_;
  std::map<uint32_t, proxy::PluginDispatcher*> plugin_dispatchers_;
  uint32_t next_plugin_dispatcher_id_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WaitableEvent* shutdown_event_;
  IPC::ChannelHandle renderer_ipc_handle_;
  std::unique_ptr<IPC::SyncChannel> channel_;
};

}  // namespace ppapi

#endif  // PPAPI_NACL_IRT_PPAPI_DISPATCHER_H_
