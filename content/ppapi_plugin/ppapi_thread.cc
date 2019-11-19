// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_thread.h"

#include <stddef.h>

#include <limits>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "content/child/browser_font_resource_trusted.h"
#include "content/child/child_process.h"
#include "content/ppapi_plugin/broker_process_dispatcher.h"
#include "content/ppapi_plugin/plugin_process_dispatcher.h"
#include "content/ppapi_plugin/ppapi_blink_platform_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
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

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "content/child/font_warmup_win.h"
#include "sandbox/win/src/sandbox.h"
#endif

#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt_exec.h"
#endif

#if defined(OS_WIN)
extern sandbox::TargetServices* g_target_services;

// Used by EnumSystemLocales for warming up.
static BOOL CALLBACK EnumLocalesProcEx(
    LPWSTR lpLocaleString,
    DWORD dwFlags,
    LPARAM lParam) {
  return TRUE;
}

// Warm up language subsystems before the sandbox is turned on.
static void WarmupWindowsLocales(const ppapi::PpapiPermissions& permissions) {
  ::GetUserDefaultLangID();
  ::GetUserDefaultLCID();

  if (permissions.HasPermission(ppapi::PERMISSION_FLASH))
    ::EnumSystemLocalesEx(EnumLocalesProcEx, LOCALE_WINDOWS, 0, 0);
}

#endif

namespace content {

typedef int32_t (*InitializeBrokerFunc)
    (PP_ConnectInstance_Func* connect_instance_func);

PpapiThread::PpapiThread(base::RepeatingClosure quit_closure,
                         const base::CommandLine& command_line,
                         bool is_broker)
    : ChildThreadImpl(std::move(quit_closure)),
      is_broker_(is_broker),
      plugin_globals_(GetIOTaskRunner()),
      connect_instance_func_(nullptr),
      local_pp_module_(base::RandInt(0, std::numeric_limits<PP_Module>::max())),
      next_plugin_dispatcher_id_(1) {
  plugin_globals_.SetPluginProxyDelegate(this);
  plugin_globals_.set_command_line(
      command_line.GetSwitchValueASCII(switches::kPpapiFlashArgs));

  blink_platform_impl_.reset(new PpapiBlinkPlatformImpl);
  blink::Platform::CreateMainThreadAndInitialize(blink_platform_impl_.get());

  if (!is_broker_) {
    scoped_refptr<ppapi::proxy::PluginMessageFilter> plugin_filter(
        new ppapi::proxy::PluginMessageFilter(
            nullptr, plugin_globals_.resource_reply_thread_registrar()));
    channel()->AddFilter(plugin_filter.get());
    plugin_globals_.RegisterResourceMessageFilters(plugin_filter.get());
  }

  // In single process, browser main loop set up the discardable memory
  // allocator.
  if (!command_line.HasSwitch(switches::kSingleProcess)) {
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager>
        manager_remote;
    ChildThread::Get()->BindHostReceiver(
        manager_remote.InitWithNewPipeAndPassReceiver());
    discardable_shared_memory_manager_ = std::make_unique<
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
    IPC_MESSAGE_HANDLER(PpapiMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(PpapiMsg_Hang, OnHang)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PpapiThread::OnChannelConnected(int32_t peer_pid) {
  ChildThreadImpl::OnChannelConnected(peer_pid);
#if defined(OS_WIN)
  if (is_broker_)
    peer_handle_.Set(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, peer_pid));
#endif
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

void PpapiThread::PreCacheFontForFlash(const void* logfontw) {
#if defined(OS_WIN)
  ChildThreadImpl::PreCacheFont(*static_cast<const LOGFONTW*>(logfontw));
#endif
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
  static auto* ppapi_path_key = base::debug::AllocateCrashKeyString(
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
  std::vector<PepperPluginInfo> plugins;
  GetContentClient()->AddPepperPlugins(&plugins);
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
    base::TimeDelta load_time;
    {
      TRACE_EVENT1("ppapi", "PpapiThread::LoadPlugin", "path",
                   path.MaybeAsASCII());

      base::TimeTicks start = base::TimeTicks::Now();
      library = base::ScopedNativeLibrary(path);
      load_time = base::TimeTicks::Now() - start;
    }

    if (!library.is_valid()) {
      LOG(ERROR) << "Failed to load Pepper module from " << path.value()
                 << " (error: " << library.GetError()->ToString() << ")";
      if (!base::PathExists(path)) {
        ReportLoadResult(path, FILE_MISSING);
        return;
      }
      ReportLoadResult(path, LOAD_FAILED);
      // Report detailed reason for load failure.
      ReportLoadErrorCode(path, library.GetError());
      return;
    }

    // Only report load time for success loads.
    ReportLoadTime(path, load_time);

    // Get the GetInterface function (required).
    plugin_entry_points_.get_interface =
        reinterpret_cast<PP_GetInterface_Func>(
            library.GetFunctionPointer("PPP_GetInterface"));
    if (!plugin_entry_points_.get_interface) {
      LOG(WARNING) << "No PPP_GetInterface in plugin library";
      ReportLoadResult(path, ENTRY_POINT_MISSING);
      return;
    }

    // The ShutdownModule/ShutdownBroker function is optional.
    plugin_entry_points_.shutdown_module =
        is_broker_ ?
        reinterpret_cast<PP_ShutdownModule_Func>(
            library.GetFunctionPointer("PPP_ShutdownBroker")) :
        reinterpret_cast<PP_ShutdownModule_Func>(
            library.GetFunctionPointer("PPP_ShutdownModule"));

    if (!is_broker_) {
      // Get the InitializeModule function (required for non-broker code).
      plugin_entry_points_.initialize_module =
          reinterpret_cast<PP_InitializeModule_Func>(
              library.GetFunctionPointer("PPP_InitializeModule"));
      if (!plugin_entry_points_.initialize_module) {
        LOG(WARNING) << "No PPP_InitializeModule in plugin library";
        ReportLoadResult(path, ENTRY_POINT_MISSING);
        return;
      }
    }
  }

#if defined(OS_WIN)
  // If code subsequently tries to exit using abort(), force a crash (since
  // otherwise these would be silent terminations and fly under the radar).
  base::win::SetAbortBehaviorForCrashReporting();

  // Once we lower the token the sandbox is locked down and no new modules
  // can be loaded. TODO(cpu): consider changing to the loading style of
  // regular plugins.
  if (g_target_services) {
    if (permissions.HasPermission(ppapi::PERMISSION_FLASH)) {
      // Let Flash load DXVA before lockdown.
      LoadLibraryA("dxva2.dll");

      base::CPU cpu;
      if (cpu.vendor_name() == "AuthenticAMD") {
        // The AMD crypto acceleration is only AMD Bulldozer and above.
#if defined(_WIN64)
        LoadLibraryA("amdhcp64.dll");
#else
        LoadLibraryA("amdhcp32.dll");
#endif
      }
    }

    // Cause advapi32 to load before the sandbox is turned on.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);

    WarmupWindowsLocales(permissions);

    if (!base::win::IsUser32AndGdi32Available() &&
        permissions.HasPermission(ppapi::PERMISSION_FLASH)) {
      PatchGdiFontEnumeration(path);
    }

    g_target_services->LowerToken();
  }
#endif

  if (is_broker_) {
    // Get the InitializeBroker function (required).
    InitializeBrokerFunc init_broker =
        reinterpret_cast<InitializeBrokerFunc>(
            library.GetFunctionPointer("PPP_InitializeBroker"));
    if (!init_broker) {
      LOG(WARNING) << "No PPP_InitializeBroker in plugin library";
      ReportLoadResult(path, ENTRY_POINT_MISSING);
      return;
    }

    int32_t init_error = init_broker(&connect_instance_func_);
    if (init_error != PP_OK) {
      LOG(WARNING) << "InitBroker failed with error " << init_error;
      ReportLoadResult(path, INIT_FAILED);
      return;
    }
    if (!connect_instance_func_) {
      LOG(WARNING) << "InitBroker did not provide PP_ConnectInstance_Func";
      ReportLoadResult(path, INIT_FAILED);
      return;
    }
  } else {
    int32_t init_error = plugin_entry_points_.initialize_module(
        local_pp_module_, &ppapi::proxy::PluginDispatcher::GetBrowserInterface);
    if (init_error != PP_OK) {
      LOG(WARNING) << "InitModule failed with error " << init_error;
      ReportLoadResult(path, INIT_FAILED);
      return;
    }
  }

  // Initialization succeeded, so keep the plugin DLL loaded.
  library_ = std::move(library);

  ReportLoadResult(path, LOAD_SUCCESS);
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

void PpapiThread::OnCrash() {
  // Intentionally crash upon the request of the browser.
  //
  // Linker's ICF feature may merge this function with other functions with the
  // same definition and it may confuse the crash report processing system.
  static int static_variable_to_make_this_function_unique = 0;
  base::debug::Alias(&static_variable_to_make_this_function_unique);

  volatile int* null_pointer = nullptr;
  *null_pointer = 0;
}

void PpapiThread::OnHang() {
  // Intentionally hang upon the request of the browser.
  for (;;)
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
}

bool PpapiThread::SetupChannel(base::ProcessId renderer_pid,
                               int renderer_child_id,
                               bool incognito,
                               IPC::ChannelHandle* handle) {
  DCHECK(is_broker_ == (connect_instance_func_ != nullptr));
  mojo::MessagePipe pipe;

  ppapi::proxy::ProxyChannel* dispatcher = nullptr;
  bool init_result = false;
  if (is_broker_) {
    bool peer_is_browser = renderer_pid == base::kNullProcessId;
    BrokerProcessDispatcher* broker_dispatcher =
        new BrokerProcessDispatcher(plugin_entry_points_.get_interface,
                                    connect_instance_func_, peer_is_browser);
    init_result = broker_dispatcher->InitBrokerWithChannel(
        this, renderer_pid, pipe.handle0.release(), false);
    dispatcher = broker_dispatcher;
  } else {
    DCHECK_NE(base::kNullProcessId, renderer_pid);
    PluginProcessDispatcher* plugin_dispatcher =
        new PluginProcessDispatcher(plugin_entry_points_.get_interface,
                                    permissions_,
                                    incognito);
    init_result = plugin_dispatcher->InitPluginWithChannel(
        this, renderer_pid, pipe.handle0.release(), false);
    dispatcher = plugin_dispatcher;
  }

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

static std::string GetHistogramName(bool is_broker,
                                    const std::string& metric_name,
                                    const base::FilePath& path) {
  return std::string("Plugin.Ppapi") + (is_broker ? "Broker" : "Plugin") +
         metric_name + "_" + path.BaseName().MaybeAsASCII();
}

void PpapiThread::ReportLoadResult(const base::FilePath& path,
                                   LoadResult result) {
  DCHECK_LT(result, LOAD_RESULT_MAX);

  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          GetHistogramName(is_broker_, "LoadResult", path),
          1,
          LOAD_RESULT_MAX,
          LOAD_RESULT_MAX + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->Add(result);
}

void PpapiThread::ReportLoadErrorCode(
    const base::FilePath& path,
    const base::NativeLibraryLoadError* error) {
// Only report load error code on Windows because that's the only platform that
// has a numerical error value.
#if defined(OS_WIN)
  base::UmaHistogramSparse(GetHistogramName(is_broker_, "LoadErrorCode", path),
                           error->code);
#endif
}

void PpapiThread::ReportLoadTime(const base::FilePath& path,
                                 const base::TimeDelta load_time) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::Histogram::FactoryTimeGet(
          GetHistogramName(is_broker_, "LoadTime", path),
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromSeconds(10),
          50,
          base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->AddTime(load_time);
}

}  // namespace content
