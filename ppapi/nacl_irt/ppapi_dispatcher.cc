// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/ppapi_dispatcher.h"

#include <stddef.h>

#include <map>
#include <set>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/nacl_irt/manifest_service.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"

namespace ppapi {

PpapiDispatcher::PpapiDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WaitableEvent* shutdown_event,
    IPC::ChannelHandle browser_ipc_handle,
    IPC::ChannelHandle renderer_ipc_handle)
    : next_plugin_dispatcher_id_(0),
      task_runner_(io_task_runner),
      shutdown_event_(shutdown_event),
      renderer_ipc_handle_(renderer_ipc_handle) {
  proxy::PluginGlobals* globals = proxy::PluginGlobals::Get();
  // Delay initializing the SyncChannel until after we add filters. This
  // ensures that the filters won't miss any messages received by
  // the channel.
  channel_ = IPC::SyncChannel::Create(
      this, GetIPCTaskRunner(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), GetShutdownEvent());
  scoped_refptr<ppapi::proxy::PluginMessageFilter> plugin_filter(
      new ppapi::proxy::PluginMessageFilter(
          NULL, globals->resource_reply_thread_registrar()));
  channel_->AddFilter(plugin_filter.get());
  globals->RegisterResourceMessageFilters(plugin_filter.get());

  channel_->Init(browser_ipc_handle, IPC::Channel::MODE_SERVER, true);
}

base::SingleThreadTaskRunner* PpapiDispatcher::GetIPCTaskRunner() {
  return task_runner_.get();
}

base::WaitableEvent* PpapiDispatcher::GetShutdownEvent() {
  return shutdown_event_;
}

IPC::PlatformFileForTransit PpapiDispatcher::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId peer_pid,
    bool should_close_source) {
  return IPC::InvalidPlatformFileForTransit();
}

base::UnsafeSharedMemoryRegion
PpapiDispatcher::ShareUnsafeSharedMemoryRegionWithRemote(
    const base::UnsafeSharedMemoryRegion& region,
    base::ProcessId remote_pid) {
  return base::UnsafeSharedMemoryRegion();
}

base::ReadOnlySharedMemoryRegion
PpapiDispatcher::ShareReadOnlySharedMemoryRegionWithRemote(
    const base::ReadOnlySharedMemoryRegion& region,
    base::ProcessId remote_pid) {
  return base::ReadOnlySharedMemoryRegion();
}

std::set<PP_Instance>* PpapiDispatcher::GetGloballySeenInstanceIDSet() {
  return &instances_;
}

uint32_t PpapiDispatcher::Register(proxy::PluginDispatcher* plugin_dispatcher) {
  if (!plugin_dispatcher ||
      plugin_dispatchers_.size() >= std::numeric_limits<uint32_t>::max()) {
    return 0;
  }

  uint32_t id = 0;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble
    // when the counter overflows.
    id = next_plugin_dispatcher_id_++;
  } while (id == 0 ||
           plugin_dispatchers_.find(id) != plugin_dispatchers_.end());
  plugin_dispatchers_[id] = plugin_dispatcher;
  return id;
}

void PpapiDispatcher::Unregister(uint32_t plugin_dispatcher_id) {
  plugin_dispatchers_.erase(plugin_dispatcher_id);
}

IPC::Sender* PpapiDispatcher::GetBrowserSender() {
  return this;
}

std::string PpapiDispatcher::GetUILanguage() {
  NOTIMPLEMENTED();
  return std::string();
}

void PpapiDispatcher::SetActiveURL(const std::string& url) {
  NOTIMPLEMENTED();
}

PP_Resource PpapiDispatcher::CreateBrowserFont(
    proxy::Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const Preferences& prefs) {
  NOTIMPLEMENTED();
  return 0;
}

bool PpapiDispatcher::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PpapiDispatcher, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_InitializeNaClDispatcher,
                        OnMsgInitializeNaClDispatcher)
    // All other messages are simply forwarded to a PluginDispatcher.
    IPC_MESSAGE_UNHANDLED(OnPluginDispatcherMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return true;
}

void PpapiDispatcher::OnChannelError() {
  exit(1);
}

bool PpapiDispatcher::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void PpapiDispatcher::OnMsgInitializeNaClDispatcher(
    const PpapiNaClPluginArgs& args) {
  static bool command_line_and_logging_initialized = false;
  CHECK(!command_line_and_logging_initialized)
      << "InitializeNaClDispatcher must be called once per plugin.";

  command_line_and_logging_initialized = true;
  base::CommandLine::Init(0, NULL);
  for (size_t i = 0; i < args.switch_names.size(); ++i) {
    DCHECK(i < args.switch_values.size());
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        args.switch_names[i], args.switch_values[i]);
  }
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::InitInstance(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEnableFeatures),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDisableFeatures));

  // Tell the process-global GetInterface which interfaces it can return to the
  // plugin.
  proxy::InterfaceList::SetProcessGlobalPermissions(args.permissions);

  int32_t error = ::PPP_InitializeModule(
      0 /* module */,
      &proxy::PluginDispatcher::GetBrowserInterface);
  if (error)
    ::exit(error);

  proxy::PluginDispatcher* dispatcher =
      new proxy::PluginDispatcher(::PPP_GetInterface, args.permissions,
                                  args.off_the_record);
  if (!dispatcher->InitPluginWithChannel(this, base::kNullProcessId,
                                         renderer_ipc_handle_, false)) {
    delete dispatcher;
    return;
  }
  // From here, the dispatcher will manage its own lifetime according to the
  // lifetime of the attached channel.

  // Notify the renderer process, if necessary.
  ManifestService* manifest_service = GetManifestService();
  if (manifest_service)
    manifest_service->StartupInitializationComplete();
}

void PpapiDispatcher::OnPluginDispatcherMessageReceived(
    const IPC::Message& msg) {
  // The first parameter should be a plugin dispatcher ID.
  base::PickleIterator iter(msg);
  uint32_t id = 0;
  if (!iter.ReadUInt32(&id)) {
    NOTREACHED();
  }
  std::map<uint32_t, proxy::PluginDispatcher*>::iterator dispatcher =
      plugin_dispatchers_.find(id);
  if (dispatcher != plugin_dispatchers_.end())
    dispatcher->second->OnMessageReceived(msg);
}

}  // namespace ppapi
