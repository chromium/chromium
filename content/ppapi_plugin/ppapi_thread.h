// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/trusted/ppp_broker.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {
class CommandLine;
class FilePath;
}

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace IPC {
struct ChannelHandle;
}

namespace content {

class PpapiBlinkPlatformImpl;

#if defined(COMPILER_MSVC)
// See explanation for other RenderViewHostImpl which is the same issue.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

class PpapiThread : public ChildThreadImpl,
                    public ppapi::proxy::PluginDispatcher::PluginDelegate,
                    public ppapi::proxy::PluginProxyDelegate {
 public:
  PpapiThread(base::RepeatingClosure quit_closure,
              const base::CommandLine& command_line,
              bool is_broker);
  ~PpapiThread() override;
  void Shutdown() override;

 private:
  // Make sure the enum list in tools/histogram/histograms.xml is updated with
  // any change in this list.
  enum LoadResult {
    LOAD_SUCCESS,
    LOAD_FAILED,
    ENTRY_POINT_MISSING,
    INIT_FAILED,
    FILE_MISSING,
    // NOTE: Add new values only immediately above this line.
    LOAD_RESULT_MAX  // Boundary value for UMA_HISTOGRAM_ENUMERATION.
  };

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
  void PreCacheFontForFlash(const void* logfontw) override;
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
  void OnCrash();
  void OnHang();

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

  void ReportLoadResult(const base::FilePath& path, LoadResult result);

  // Reports |error| to UMA when plugin load fails.
  void ReportLoadErrorCode(const base::FilePath& path,
                           const base::NativeLibraryLoadError* error);

  // Reports time to load the plugin.
  void ReportLoadTime(const base::FilePath& path,
                      const base::TimeDelta load_time);

  // True if running in a broker process rather than a normal plugin process.
  bool is_broker_;

  base::ScopedNativeLibrary library_;

  ppapi::PpapiPermissions permissions_;

  // Global state tracking for the proxy.
  ppapi::proxy::PluginGlobals plugin_globals_;

  // Storage for plugin entry points.
  PepperPluginInfo::EntryPoints plugin_entry_points_;

  // Callback to call when a new instance connects to the broker.
  // Used only when is_broker_.
  PP_ConnectInstance_Func connect_instance_func_;

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
  std::map<uint32_t, ppapi::proxy::PluginDispatcher*> plugin_dispatchers_;
  uint32_t next_plugin_dispatcher_id_;

  // The BlinkPlatformImpl implementation.
  std::unique_ptr<PpapiBlinkPlatformImpl> blink_platform_impl_;

#if defined(OS_WIN)
  // Caches the handle to the peer process if this is a broker.
  base::win::ScopedHandle peer_handle_;
#endif

  std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PpapiThread);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
