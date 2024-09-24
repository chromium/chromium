// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_process_host.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_sandboxed_process_launcher_delegate.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_plugin_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/font_render_params.h"
#endif

namespace content {

class PpapiPluginProcessHost::PluginNetworkObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit PluginNetworkObserver(PpapiPluginProcessHost* process_host)
      : process_host_(process_host), network_connection_tracker_(nullptr) {
    GetNetworkConnectionTrackerFromUIThread(
        base::BindOnce(&PluginNetworkObserver::SetNetworkConnectionTracker,
                       weak_factory_.GetWeakPtr()));
  }

  void SetNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker) {
    DCHECK(network_connection_tracker);
    network_connection_tracker_ = network_connection_tracker;
    network_connection_tracker_->AddNetworkConnectionObserver(this);
  }

  ~PluginNetworkObserver() override {
    if (network_connection_tracker_)
      network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  }

  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    process_host_->Send(new PpapiMsg_SetNetworkState(
        type != network::mojom::ConnectionType::CONNECTION_NONE));
  }

 private:
  const raw_ptr<PpapiPluginProcessHost> process_host_;
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;
  base::WeakPtrFactory<PluginNetworkObserver> weak_factory_{this};
};

PpapiPluginProcessHost::~PpapiPluginProcessHost() {
  DVLOG(1) << "PpapiPluginProcessHost"
           << "~PpapiPluginProcessHost()";
  CancelRequests();
}

// static
PpapiPluginProcessHost* PpapiPluginProcessHost::CreatePluginHost(
    const ContentPluginInfo& info,
    const base::FilePath& profile_data_directory,
    const std::optional<url::Origin>& origin_lock) {
  PpapiPluginProcessHost* plugin_host =
      new PpapiPluginProcessHost(info, profile_data_directory, origin_lock);
  if (plugin_host->Init(info))
    return plugin_host;

  NOTREACHED_IN_MIGRATION();  // Init is not expected to fail.
  return nullptr;
}

// static
void PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
    int plugin_process_id,
    int32_t pp_instance,
    const PepperRendererInstanceData& instance_data) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() &&
        iter->process_->GetData().id == plugin_process_id) {
      // Found the plugin.
      iter->host_impl_->AddInstance(pp_instance, instance_data);
      return;
    }
  }
  // We'll see this passed with a 0 process ID for the browser tag stuff that
  // is currently in the process of being removed.
  //
  // TODO(brettw) When old browser tag impl is removed
  // (PepperPluginDelegateImpl::CreateBrowserPluginModule passes a 0 plugin
  // process ID) this should be converted to a NOTREACHED().
  DCHECK(plugin_process_id == 0)
      << "Renderer sent a bad plugin process host ID";
}

// static
void PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(
    int plugin_process_id,
    int32_t pp_instance) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() &&
        iter->process_->GetData().id == plugin_process_id) {
      // Found the plugin.
      iter->host_impl_->DeleteInstance(pp_instance);
      return;
    }
  }
  // Note: It's possible that the plugin process has already been deleted by
  // the time this message is received. For example, it could have crashed.
  // That's OK, we can just ignore this message.
}

// static
void PpapiPluginProcessHost::FindByName(
    const std::u16string& name,
    std::vector<PpapiPluginProcessHost*>* hosts) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->process_.get() && iter->process_->GetData().name == name)
      hosts->push_back(*iter);
  }
}

bool PpapiPluginProcessHost::Send(IPC::Message* message) {
  return process_->Send(message);
}

void PpapiPluginProcessHost::OpenChannelToPlugin(Client* client) {
  if (process_->GetHost()->IsChannelOpening()) {
    // The channel is already in the process of being opened.  Put
    // this "open channel" request into a queue of requests that will
    // be run once the channel is open.
    pending_requests_.push_back(client);
    return;
  }

  // We already have an open channel, send a request right away to plugin.
  RequestPluginChannel(client);
}

PpapiPluginProcessHost::PpapiPluginProcessHost(
    const ContentPluginInfo& info,
    const base::FilePath& profile_data_directory,
    const std::optional<url::Origin>& origin_lock)
    : profile_data_directory_(profile_data_directory),
      origin_lock_(origin_lock) {
  uint32_t base_permissions = info.permissions;

  // We don't have to do any whitelisting for APIs in this process host, so
  // don't bother passing a browser context or document url here.
  if (GetContentClient()->browser()->IsPluginAllowedToUseDevChannelAPIs(nullptr,
                                                                        GURL()))
    base_permissions |= ppapi::PERMISSION_DEV_CHANNEL;
  permissions_ = ppapi::PpapiPermissions::GetForCommandLine(base_permissions);

  process_ = std::make_unique<BrowserChildProcessHostImpl>(
      PROCESS_TYPE_PPAPI_PLUGIN, this, ChildProcessHost::IpcMode::kNormal);

  host_impl_ = std::make_unique<BrowserPpapiHostImpl>(
      this, permissions_, info.name, info.path, profile_data_directory,
      false /* in_process */, false /* external_plugin */);

  process_->GetHost()->AddFilter(host_impl_->message_filter().get());

  GetContentClient()->browser()->DidCreatePpapiPlugin(host_impl_.get());

  // Only request network status updates if the plugin has dev permissions.
  if (permissions_.HasPermission(ppapi::PERMISSION_DEV))
    network_observer_ = std::make_unique<PluginNetworkObserver>(this);
}

bool PpapiPluginProcessHost::Init(const ContentPluginInfo& info) {
  plugin_path_ = info.path;
  if (info.name.empty()) {
    process_->SetName(plugin_path_.BaseName().LossyDisplayName());
  } else {
    process_->SetName(base::UTF8ToUTF16(info.name));
  }

  process_->GetHost()->CreateChannelMojo();

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPpapiPluginLauncher);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  int flags = plugin_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                        ChildProcessHost::CHILD_NORMAL;
#else
  // Plugins can't generate executable code.
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif
  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty()) {
    VLOG(1) << "Pepper plugin exe path is empty.";
    return false;
  }

  std::unique_ptr<base::CommandLine> cmd_line =
      std::make_unique<base::CommandLine>(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kPpapiPluginProcess);

#if BUILDFLAG(IS_WIN)
  cmd_line->AppendArgNative(
      app_launch_prefetch::GetPrefetchSwitch(app_launch_prefetch::SubprocessType::kPpapi));
#endif  // BUILDFLAG(IS_WIN)

  // These switches are forwarded to plugin pocesses.
  static const char* const kCommonForwardSwitches[] = {
    switches::kVModule
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kCommonForwardSwitches);

  static const char* const kPluginForwardSwitches[] = {
    sandbox::policy::switches::kDisableSeccompFilterSandbox,
    sandbox::policy::switches::kNoSandbox,
#if BUILDFLAG(IS_MAC)
    sandbox::policy::switches::kEnableSandboxLogging,
#endif
    switches::kPpapiStartupDialog,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kPluginForwardSwitches);

  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  if (!locale.empty()) {
    // Pass on the locale so the plugin will know what language we're using.
    cmd_line->AppendSwitchASCII(switches::kLang, locale);
  }

#if BUILDFLAG(IS_WIN)
  cmd_line->AppendSwitchASCII(
      switches::kDeviceScaleFactor,
      base::NumberToString(display::win::GetDPIScale()));
  const gfx::FontRenderParams font_params =
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr);
  cmd_line->AppendSwitchASCII(switches::kPpapiAntialiasedTextEnabled,
                              base::NumberToString(font_params.antialiasing));
  cmd_line->AppendSwitchASCII(
      switches::kPpapiSubpixelRenderingSetting,
      base::NumberToString(font_params.subpixel_rendering));

  LOG(WARNING) << "Ppapi sandbox on Windows is not supported.";
#endif

  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  // On posix, only use the zygote if we are not using a plugin launcher -
  // having a plugin launcher means we need to use another process instead of
  // just forking the zygote.
  process_->Launch(
      std::make_unique<PpapiPluginSandboxedProcessLauncherDelegate>(),
      std::move(cmd_line), true);
  return true;
}

void PpapiPluginProcessHost::RequestPluginChannel(Client* client) {
  base::ProcessHandle process_handle = base::kNullProcessHandle;
  int renderer_child_id = base::kNullProcessId;
  client->GetPpapiChannelInfo(&process_handle, &renderer_child_id);

  base::ProcessId process_id = base::kNullProcessId;
  if (process_handle != base::kNullProcessHandle) {
    // This channel is not used by the browser itself.
    process_id = base::GetProcId(process_handle);
    CHECK_NE(base::kNullProcessId, process_id);
  }

  // We can't send any sync messages from the browser because it might lead to
  // a hang. See the similar code in PluginProcessHost for more description.
  PpapiMsg_CreateChannel* msg = new PpapiMsg_CreateChannel(
      process_id, renderer_child_id, client->Incognito());
  msg->set_unblock(true);
  if (Send(msg)) {
    sent_requests_.push(client);
  } else {
    client->OnPpapiChannelOpened(IPC::ChannelHandle(), base::kNullProcessId, 0);
  }
}

void PpapiPluginProcessHost::OnProcessLaunched() {
  VLOG(2) << "ppapi plugin process launched.";
  host_impl_->set_plugin_process(process_->GetProcess().Duplicate());
}

void PpapiPluginProcessHost::OnProcessCrashed(int exit_code) {
  VLOG(1) << "ppapi plugin process crashed.";
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PluginServiceImpl::RegisterPluginCrash,
                     base::Unretained(PluginServiceImpl::GetInstance()),
                     plugin_path_));
}

bool PpapiPluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiPluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ChannelCreated,
                        OnRendererPluginChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

// Called when the browser <--> plugin channel has been established.
void PpapiPluginProcessHost::OnChannelConnected(int32_t peer_pid) {
  // This will actually load the plugin. Errors will actually not be reported
  // back at this point. Instead, the plugin will fail to establish the
  // connections when we request them on behalf of the renderer(s).
  Send(new PpapiMsg_LoadPlugin(plugin_path_, permissions_));

  // Process all pending channel requests from the renderers.
  for (size_t i = 0; i < pending_requests_.size(); i++)
    RequestPluginChannel(pending_requests_[i]);
  pending_requests_.clear();
}

// Called when the browser <--> plugin channel has an error. This normally
// means the plugin has crashed.
void PpapiPluginProcessHost::OnChannelError() {
  VLOG(1) << "PpapiPluginProcessHost"
          << "::OnChannelError()";
  // We don't need to notify the renderers that were communicating with the
  // plugin since they have their own channels which will go into the error
  // state at the same time. Instead, we just need to notify any renderers
  // that have requested a connection but have not yet received one.
  CancelRequests();
}

void PpapiPluginProcessHost::CancelRequests() {
  DVLOG(1) << "PpapiPluginProcessHost"
           << "CancelRequests()";
  for (size_t i = 0; i < pending_requests_.size(); i++) {
    pending_requests_[i]->OnPpapiChannelOpened(IPC::ChannelHandle(),
                                               base::kNullProcessId, 0);
  }
  pending_requests_.clear();

  while (!sent_requests_.empty()) {
    sent_requests_.front()->OnPpapiChannelOpened(IPC::ChannelHandle(),
                                                 base::kNullProcessId, 0);
    sent_requests_.pop();
  }
}

// Called when a new plugin <--> renderer channel has been created.
void PpapiPluginProcessHost::OnRendererPluginChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  if (sent_requests_.empty())
    return;

  // All requests should be processed FIFO, so the next item in the
  // sent_requests_ queue should be the one that the plugin just created.
  Client* client = sent_requests_.front();
  sent_requests_.pop();

  const ChildProcessData& data = process_->GetData();
  client->OnPpapiChannelOpened(channel_handle, data.GetProcess().Pid(),
                               data.id);
}

}  // namespace content
