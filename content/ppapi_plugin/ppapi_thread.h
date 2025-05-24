// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/scoped_native_library.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/content_plugin_info.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {
class CommandLine;
class FilePath;
class WaitableEvent;
}

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace IPC {
struct ChannelHandle;
}

namespace content {

class PpapiBlinkPlatformImpl;

class PpapiThread : public ChildThreadImpl,
                    public ppapi::proxy::PluginDispatcher::PluginDelegate,
                    public ppapi::proxy::PluginProxyDelegate {
 public:
  PpapiThread() = delete;

  PpapiThread(base::RepeatingClosure quit_closure,
              const base::CommandLine& command_line);

  PpapiThread(const PpapiThread&) = delete;
  PpapiThread& operator=(const PpapiThread&) = delete;

  ~PpapiThread() override;

  void Shutdown() override;

 private:
  // ChildThread overrides.
  bool Send(IPC::Message* msg) override;
  bool OnControlMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;

  // PluginDispatcher::PluginDelegate implementation.
  std::set<PP_Instance>* GetGloballySeenInstanceIDSet() override;
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
  uint32_t Register(ppapi::proxy::PluginDispatcher* plugin_dispatcher) override;
  void Unregister(uint32_t plugin_dispatcher_id) override;

  // PluginProxyDelegate.
  // SendToBrowser() is intended to be safe to use on another thread so
  // long as the main PpapiThread outlives it.
  IPC::Sender* GetBrowserSender() override;
  std::string GetUILanguage() override;
  void SetActiveURL(const std::string& url) override;
  PP_Resource CreateBrowserFont(ppapi::proxy::Connection connection,
                                PP_Instance instance,
                                const PP_BrowserFont_Trusted_Description& desc,
                                const ppapi::Preferences& prefs) override;

  // Message handlers.
  void OnLoadPlugin(const base::FilePath& path,
                    const ppapi::PpapiPermissions& permissions);
  void OnCreateChannel(base::ProcessId renderer_pid,
                       int renderer_child_id,
                       bool incognito);
  void OnSetNetworkState(bool online);

  // Sets up the channel to the given renderer. If |renderer_pid| is
  // base::kNullProcessId, the channel is set up to the browser. On success,
  // returns true and fills the given ChannelHandle with the information from
  // the new channel.
  bool SetupChannel(base::ProcessId renderer_pid,
                    int renderer_child_id,
                    bool incognito,
                    IPC::ChannelHandle* handle);

  // Sets up the name of the plugin for logging using the given path.
  void SavePluginName(const base::FilePath& path);

  base::ScopedNativeLibrary library_;

  ppapi::PpapiPermissions permissions_;

  // Global state tracking for the proxy.
  ppapi::proxy::PluginGlobals plugin_globals_;

  // Storage for plugin entry points.
  ContentPluginInfo::EntryPoints plugin_entry_points_;

  // Local concept of the module ID. Some functions take this. It's necessary
  // for the in-process PPAPI to handle this properly, but for proxied it's
  // unnecessary. The proxy talking to multiple renderers means that each
  // renderer has a different idea of what the module ID is for this plugin.
  // To force people to "do the right thing" we generate a random module ID
  // and pass it around as necessary.
  PP_Module local_pp_module_;

  // See Dispatcher::Delegate::GetGloballySeenInstanceIDSet.
  std::set<PP_Instance> globally_seen_instance_ids_;

  // The PluginDispatcher instances contained in the map are not owned by it.
  std::map<uint32_t, raw_ptr<ppapi::proxy::PluginDispatcher, CtnExperimental>>
      plugin_dispatchers_;
  uint32_t next_plugin_dispatcher_id_;

  // The BlinkPlatformImpl implementation.
  std::unique_ptr<PpapiBlinkPlatformImpl> blink_platform_impl_;

  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
