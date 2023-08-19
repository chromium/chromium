// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_thread.h"

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "content/child/browser_font_resource_trusted.h"
#include "content/child/child_process.h"
#include "content/ppapi_plugin/plugin_process_dispatcher.h"
#include "content/ppapi_plugin/ppapi_blink_platform_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_plugin_info.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/c/dev/ppp_network_state_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "third_party/blink/public/web/blink.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "content/child/font_warmup_win.h"
#include "sandbox/policy/win/sandbox_warmup.h"
#include "sandbox/win/src/sandbox.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "sandbox/mac/seatbelt_exec.h"
#endif

#if BUILDFLAG(IS_WIN)
extern sandbox::TargetServices* g_target_services;

// Warm up language subsystems before the sandbox is turned on.
static void WarmupWindowsLocales(const ppapi::PpapiPermissions& permissions) {
  ::GetUserDefaultLangID();
  ::GetUserDefaultLCID();
}

#endif

namespace content {

PpapiThread::PpapiThread(base::RepeatingClosure quit_closure,
                         const base::CommandLine& command_line)
    : ChildThreadImpl(std::move(quit_closure)),
      plugin_globals_(GetIOTaskRunner()),
      local_pp_module_(base::RandInt(0, std::numeric_limits<PP_Module>::max())),
      next_plugin_dispatcher_id_(1) {
  plugin_globals_.SetPluginProxyDelegate(this);

  blink_platform_impl_ = std::make_unique<PpapiBlinkPlatformImpl>();
  blink::Platform::CreateMainThreadAndInitialize(blink_platform_impl_.get());

  scoped_refptr<ppapi::proxy::PluginMessageFilter> plugin_filter(
      new ppapi::proxy::PluginMessageFilter(
          nullptr, plugin_globals_.resource_reply_thread_registrar()));
  channel()->AddFilter(plugin_filter.get());
  plugin_globals_.RegisterResourceMessageFilters(plugin_filter.get());

  // In single process, browser main loop set up the discardable memory
  // allocator.
  if (!command_line.HasSwitch(switches::kSingleProcess)) {
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager>
        manager_remote;
    ChildThread::Get()->BindHostReceiver(
        manager_remote.InitWithNewPipeAndPassReceiver());
    discardable_shared_memory_manager_ = base::MakeRefCounted<
        discardable_memory::ClientDiscardableSharedMemoryManager>(
        std::move(manager_remote), GetIOTaskRunner());
    base::DiscardableMemoryAllocator::SetInstance(
        discardable_shared_memory_manager_.get());
  }
}

PpapiThread::~PpapiThread() {
}

void PpapiThread::Shutdown() {
  ChildThreadImpl::Shutdown();

  ppapi::proxy::PluginGlobals::Get()->ResetPluginProxyDelegate();
  if (plugin_entry_points_.shutdown_module)
    plugin_entry_points_.shutdown_module();
  blink_platform_impl_->Shutdown();
}

bool PpapiThread::Send(IPC::Message* msg) {
  // Allow access from multiple threads.
  if (main_thread_runner()->BelongsToCurrentThread())
    return ChildThreadImpl::Send(msg);

  return sync_message_filter()->Send(msg);
}

// Note that this function is called only for messages from the channel to the
// browser process. Messages from the renderer process are sent via a different
// channel that ends up at Dispatcher::OnMessageReceived.
bool PpapiThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiThread, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_LoadPlugin, OnLoadPlugin)
    IPC_MESSAGE_HANDLER(PpapiMsg_CreateChannel, OnCreateChannel)
    IPC_MESSAGE_HANDLER(PpapiMsg_SetNetworkState, OnSetNetworkState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PpapiThread::OnChannelConnected(int32_t peer_pid) {
  ChildThreadImpl::OnChannelConnected(peer_pid);
}

base::SingleThreadTaskRunner* PpapiThread::GetIPCTaskRunner() {
  return ChildProcess::current()->io_task_runner();
}

base::WaitableEvent* PpapiThread::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

IPC::PlatformFileForTransit PpapiThread::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId peer_pid,
    bool should_close_source) {
  return IPC::GetPlatformFileForTransit(handle, should_close_source);
}

base::UnsafeSharedMemoryRegion
PpapiThread::ShareUnsafeSharedMemoryRegionWithRemote(
    const base::UnsafeSharedMemoryRegion& region,
    base::ProcessId remote_pid) {
  DCHECK(remote_pid != base::kNullProcessId);
  return region.Duplicate();
}

base::ReadOnlySharedMemoryRegion
PpapiThread::ShareReadOnlySharedMemoryRegionWithRemote(
    const base::ReadOnlySharedMemoryRegion& region,
    base::ProcessId remote_pid) {
  DCHECK(remote_pid != base::kNullProcessId);
  return region.Duplicate();
}

std::set<PP_Instance>* PpapiThread::GetGloballySeenInstanceIDSet() {
  return &globally_seen_instance_ids_;
}

IPC::Sender* PpapiThread::GetBrowserSender() {
  return this;
}

std::string PpapiThread::GetUILanguage() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->GetSwitchValueASCII(switches::kLang);
}

void PpapiThread::SetActiveURL(const std::string& url) {
  GetContentClient()->SetActiveURL(GURL(url), std::string());
}

PP_Resource PpapiThread::CreateBrowserFont(
    ppapi::proxy::Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const ppapi::Preferences& prefs) {
  if (!BrowserFontResource_Trusted::IsPPFontDescriptionValid(desc))
    return 0;
  return (new BrowserFontResource_Trusted(
        connection, instance, desc, prefs))->GetReference();
}

uint32_t PpapiThread::Register(
    ppapi::proxy::PluginDispatcher* plugin_dispatcher) {
  if (!plugin_dispatcher ||
      plugin_dispatchers_.size() >= std::numeric_limits<uint32_t>::max()) {
    return 0;
  }

  uint32_t id = 0;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    id = next_plugin_dispatcher_id_++;
  } while (id == 0 ||
           plugin_dispatchers_.find(id) != plugin_dispatchers_.end());
  plugin_dispatchers_[id] = plugin_dispatcher;
  return id;
}

void PpapiThread::Unregister(uint32_t plugin_dispatcher_id) {
  plugin_dispatchers_.erase(plugin_dispatcher_id);
}

void PpapiThread::OnLoadPlugin(const base::FilePath& path,
                               const ppapi::PpapiPermissions& permissions) {
  // In case of crashes, the crash dump doesn't indicate which plugin
  // it came from.
  // TODO(dcheng): Would a scoped crash key be sufficient here? It's probably a
  // moot point, as this code is going to go away.
  static auto* const ppapi_path_key = base::debug::AllocateCrashKeyString(
      "ppapi_path", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(ppapi_path_key, path.MaybeAsASCII());

  SavePluginName(path);

  // This must be set before calling into the plugin so it can get the
  // interfaces it has permission for.
  ppapi::proxy::InterfaceList::SetProcessGlobalPermissions(permissions);
  permissions_ = permissions;

  // Trusted Pepper plugins may be "internal", i.e. built-in to the browser
  // binary.  If we're being asked to load such a plugin (e.g. the Chromoting
  // client) then fetch the entry points from the embedder, rather than a DLL.
  std::vector<ContentPluginInfo> plugins;
  GetContentClient()->AddPlugins(&plugins);
  for (const auto& plugin : plugins) {
    if (plugin.is_internal && plugin.path == path) {
      // An internal plugin is being loaded, so fetch the entry points.
      plugin_entry_points_ = plugin.internal_entry_points;
      break;
    }
  }

  // If the plugin isn't internal then load it from |path|.
  base::ScopedNativeLibrary library;
  if (!plugin_entry_points_.initialize_module) {
    // Load the plugin from the specified library.
    {
      TRACE_EVENT1("ppapi", "PpapiThread::LoadPlugin", "path",
                   path.MaybeAsASCII());
      library = base::ScopedNativeLibrary(path);
    }

    if (!library.is_valid()) {
      LOG(ERROR) << "Failed to load Pepper module from " << path.value()
                 << " (error: " << library.GetError()->ToString() << ")";
      return;
    }

    // Get the GetInterface function (required).
    plugin_entry_points_.get_interface =
        reinterpret_cast<PP_GetInterface_Func>(
            library.GetFunctionPointer("PPP_GetInterface"));
    if (!plugin_entry_points_.get_interface) {
      LOG(WARNING) << "No PPP_GetInterface in plugin library";
      return;
    }

    // The ShutdownModule function is optional.
    plugin_entry_points_.shutdown_module =
        reinterpret_cast<PP_ShutdownModule_Func>(
            library.GetFunctionPointer("PPP_ShutdownModule"));

    // Get the InitializeModule function.
    plugin_entry_points_.initialize_module =
        reinterpret_cast<PP_InitializeModule_Func>(
            library.GetFunctionPointer("PPP_InitializeModule"));
    if (!plugin_entry_points_.initialize_module) {
      LOG(WARNING) << "No PPP_InitializeModule in plugin library";
      return;
    }
  }

#if BUILDFLAG(IS_WIN)
  // If code subsequently tries to exit using abort(), force a crash (since
  // otherwise these would be silent terminations and fly under the radar).
  base::win::SetAbortBehaviorForCrashReporting();

  // Once we lower the token the sandbox is locked down and no new modules
  // can be loaded. TODO(cpu): consider changing to the loading style of
  // regular plugins.
  if (g_target_services) {
    sandbox::policy::WarmupRandomnessInfrastructure();

    WarmupWindowsLocales(permissions);

    g_target_services->LowerToken();
  }
#endif

  int32_t init_error = plugin_entry_points_.initialize_module(
      local_pp_module_, &ppapi::proxy::PluginDispatcher::GetBrowserInterface);
  if (init_error != PP_OK) {
    LOG(WARNING) << "InitModule failed with error " << init_error;
    return;
  }

  // Initialization succeeded, so keep the plugin DLL loaded.
  library_ = std::move(library);
}

void PpapiThread::OnCreateChannel(base::ProcessId renderer_pid,
                                  int renderer_child_id,
                                  bool incognito) {
  IPC::ChannelHandle channel_handle;

  if (!plugin_entry_points_.get_interface ||  // Plugin couldn't be loaded.
      !SetupChannel(renderer_pid, renderer_child_id, incognito,
                    &channel_handle)) {
    Send(new PpapiHostMsg_ChannelCreated(IPC::ChannelHandle()));
    return;
  }

  Send(new PpapiHostMsg_ChannelCreated(channel_handle));
}

void PpapiThread::OnSetNetworkState(bool online) {
  // Note the browser-process side shouldn't send us these messages in the
  // first unless the plugin has dev permissions, so we don't need to check
  // again here. We don't want random plugins depending on this dev interface.
  if (!plugin_entry_points_.get_interface)
    return;
  const PPP_NetworkState_Dev* ns = static_cast<const PPP_NetworkState_Dev*>(
      plugin_entry_points_.get_interface(PPP_NETWORK_STATE_DEV_INTERFACE));
  if (ns)
    ns->SetOnLine(PP_FromBool(online));
}

bool PpapiThread::SetupChannel(base::ProcessId renderer_pid,
                               int renderer_child_id,
                               bool incognito,
                               IPC::ChannelHandle* handle) {
  mojo::MessagePipe pipe;

  ppapi::proxy::ProxyChannel* dispatcher = nullptr;
  bool init_result = false;
  DCHECK_NE(base::kNullProcessId, renderer_pid);
  PluginProcessDispatcher* plugin_dispatcher = new PluginProcessDispatcher(
      plugin_entry_points_.get_interface, permissions_, incognito);
  init_result = plugin_dispatcher->InitPluginWithChannel(
      this, renderer_pid, pipe.handle0.release(), false);
  dispatcher = plugin_dispatcher;

  if (!init_result) {
    delete dispatcher;
    return false;
  }
  *handle = pipe.handle1.release();

  // From here, the dispatcher will manage its own lifetime according to the
  // lifetime of the attached channel.
  return true;
}

void PpapiThread::SavePluginName(const base::FilePath& path) {
  ppapi::proxy::PluginGlobals::Get()->set_plugin_name(
      path.BaseName().AsUTF8Unsafe());
}

}  // namespace content
