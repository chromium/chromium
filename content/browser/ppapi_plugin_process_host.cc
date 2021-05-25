// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_process_host.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/font_render_params.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/public/common/zygote/zygote_handle.h"  // nogncheck
#endif

namespace content {

// NOTE: changes to this class need to be reviewed by the security team.
class PpapiPluginSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  PpapiPluginSandboxedProcessLauncherDelegate(
      const ppapi::PpapiPermissions& permissions)
#if defined(OS_WIN)
      : permissions_(permissions)
#endif
  {
  }

  ~PpapiPluginSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    // The Pepper process is as locked-down as a renderer except that it can
    // create the server side of Chrome pipes.
    sandbox::ResultCode result;
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                             sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                             L"\\\\.\\pipe\\chrome.*");
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    content::ContentBrowserClient* browser_client =
        GetContentClient()->browser();

#if !defined(NACL_WIN64)
    // We don't support PPAPI win32k lockdown prior to Windows 10.
    if (base::win::GetVersion() >= base::win::Version::WIN10) {
      result = sandbox::policy::SandboxWin::AddWin32kLockdownPolicy(policy);
      if (result != sandbox::SBOX_ALL_OK)
        return false;
    }
#endif  // !defined(NACL_WIN64)
    const std::wstring& sid =
        browser_client->GetAppContainerSidForSandboxType(GetSandboxType());
    if (!sid.empty())
      sandbox::policy::SandboxWin::AddAppContainerPolicy(policy, sid.c_str());

    // No plugins can generate executable code.
    sandbox::MitigationFlags flags = policy->GetDelayedProcessMitigations();
    flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    if (sandbox::SBOX_ALL_OK != policy->SetDelayedProcessMitigations(flags))
      return false;

    return true;
  }
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  ZygoteHandle GetZygote() override {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringType plugin_launcher = browser_command_line
        .GetSwitchValueNative(switches::kPpapiPluginLauncher);
    if (!plugin_launcher.empty())
      return nullptr;
    return GetGenericZygote();
  }
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

  sandbox::policy::SandboxType GetSandboxType() override {
    return sandbox::policy::SandboxType::kPpapi;
  }

#if defined(OS_MAC)
  bool DisclaimResponsibility() override { return true; }
  bool EnableCpuSecurityMitigations() override { return true; }
#endif

 private:
#if defined(OS_WIN)
  const ppapi::PpapiPermissions permissions_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PpapiPluginSandboxedProcessLauncherDelegate);
};

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
  PpapiPluginProcessHost* const process_host_;
  network::NetworkConnectionTracker* network_connection_tracker_;
  base::WeakPtrFactory<PluginNetworkObserver> weak_factory_{this};
};

PpapiPluginProcessHost::~PpapiPluginProcessHost() {
  DVLOG(1) << "PpapiPluginProcessHost"
           << "~PpapiPluginProcessHost()";
  CancelRequests();
}

// static
PpapiPluginProcessHost* PpapiPluginProcessHost::CreatePluginHost(
    const PepperPluginInfo& info,
    const base::FilePath& profile_data_directory,
    const absl::optional<url::Origin>& origin_lock) {
  PpapiPluginProcessHost* plugin_host =
      new PpapiPluginProcessHost(info, profile_data_directory, origin_lock);
  if (plugin_host->Init(info))
    return plugin_host;

  NOTREACHED();  // Init is not expected to fail.
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
    const PepperPluginInfo& info,
    const base::FilePath& profile_data_directory,
    const absl::optional<url::Origin>& origin_lock)
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

bool PpapiPluginProcessHost::Init(const PepperPluginInfo& info) {
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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
  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(cmd_line.get());
  BrowserChildProcessHostImpl::CopyTraceStartupFlags(cmd_line.get());

#if defined(OS_WIN)
  cmd_line->AppendArg(switches::kPrefetchArgumentPpapi);
#endif  // defined(OS_WIN)

  // These switches are forwarded to plugin pocesses.
  static const char* const kCommonForwardSwitches[] = {
    switches::kVModule
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kCommonForwardSwitches,
                             base::size(kCommonForwardSwitches));

  static const char* const kPluginForwardSwitches[] = {
    sandbox::policy::switches::kDisableSeccompFilterSandbox,
    sandbox::policy::switches::kNoSandbox,
#if defined(OS_MAC)
    sandbox::policy::switches::kEnableSandboxLogging,
#endif
    switches::kPpapiStartupDialog,
    switches::kTimeZoneForTesting,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kPluginForwardSwitches,
                             base::size(kPluginForwardSwitches));

  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  if (!locale.empty()) {
    // Pass on the locale so the plugin will know what language we're using.
    cmd_line->AppendSwitchASCII(switches::kLang, locale);
  }

#if defined(OS_WIN)
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
#endif

  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  // On posix, only use the zygote if we are not using a plugin launcher -
  // having a plugin launcher means we need to use another process instead of
  // just forking the zygote.
  process_->Launch(
      std::make_unique<PpapiPluginSandboxedProcessLauncherDelegate>(
          permissions_),
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
