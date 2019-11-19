// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service_impl.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/plugin_list.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/pepper_plugin_list.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {
namespace {

// This enum is used to collect Flash usage data.
enum FlashUsage {
  // Number of browser processes that have started at least one PPAPI Flash
  // process during their lifetime.
  START_PPAPI_FLASH_AT_LEAST_ONCE = 1,
  // Total number of browser processes.
  TOTAL_BROWSER_PROCESSES,
  FLASH_USAGE_ENUM_COUNT
};

// Callback set on the PluginList to assert that plugin loading happens on the
// correct thread.
void WillLoadPluginsCallback(base::SequenceChecker* sequence_checker) {
  DCHECK(sequence_checker->CalledOnValidSequence());
}

}  // namespace

// static
void PluginServiceImpl::RecordBrokerUsage(int render_process_id,
                                          int render_frame_id) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_process_id, render_frame_id));
  if (web_contents) {
    ukm::SourceId source_id = static_cast<WebContentsImpl*>(web_contents)
                                  ->GetUkmSourceIdForLastCommittedSource();
    ukm::builders::Pepper_Broker(source_id).Record(ukm::UkmRecorder::Get());
  }
}

// static
PluginService* PluginService::GetInstance() {
  return PluginServiceImpl::GetInstance();
}

void PluginService::PurgePluginListCache(BrowserContext* browser_context,
                                         bool reload_pages) {
  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (!browser_context || host->GetBrowserContext() == browser_context)
      host->GetRendererInterface()->PurgePluginListCache(reload_pages);
  }
}

// static
PluginServiceImpl* PluginServiceImpl::GetInstance() {
  return base::Singleton<PluginServiceImpl>::get();
}

PluginServiceImpl::PluginServiceImpl() : filter_(nullptr) {
  // Collect the total number of browser processes (which create
  // PluginServiceImpl objects, to be precise). The number is used to normalize
  // the number of processes which start at least one NPAPI/PPAPI Flash process.
  static bool counted = false;
  if (!counted) {
    counted = true;
    UMA_HISTOGRAM_ENUMERATION("Plugin.FlashUsage", TOTAL_BROWSER_PROCESSES,
                              FLASH_USAGE_ENUM_COUNT);
  }
}

PluginServiceImpl::~PluginServiceImpl() {
}

void PluginServiceImpl::Init() {
  plugin_list_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Setup the sequence checker right after setting up the task runner.
  plugin_list_sequence_checker_.DetachFromSequence();
  PluginList::Singleton()->set_will_load_plugins_callback(base::BindRepeating(
      &WillLoadPluginsCallback, &plugin_list_sequence_checker_));

  RegisterPepperPlugins();
}

PpapiPluginProcessHost* PluginServiceImpl::FindPpapiPluginProcess(
    const base::FilePath& plugin_path,
    const base::FilePath& profile_data_directory,
    const base::Optional<url::Origin>& origin_lock) {
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->plugin_path() == plugin_path &&
        iter->profile_data_directory() == profile_data_directory &&
        (!iter->origin_lock() || iter->origin_lock() == origin_lock)) {
      return *iter;
    }
  }
  return nullptr;
}

int PluginServiceImpl::CountPpapiPluginProcessesForProfile(
    const base::FilePath& plugin_path,
    const base::FilePath& profile_data_directory) {
  int count = 0;
  for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->plugin_path() == plugin_path &&
        iter->profile_data_directory() == profile_data_directory) {
      ++count;
    }
  }
  return count;
}

PpapiPluginProcessHost* PluginServiceImpl::FindPpapiBrokerProcess(
    const base::FilePath& broker_path) {
  for (PpapiBrokerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->plugin_path() == broker_path)
      return *iter;
  }

  return nullptr;
}

PpapiPluginProcessHost* PluginServiceImpl::FindOrStartPpapiPluginProcess(
    int render_process_id,
    const base::FilePath& plugin_path,
    const base::FilePath& profile_data_directory,
    const base::Optional<url::Origin>& origin_lock) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (filter_ && !filter_->CanLoadPlugin(render_process_id, plugin_path)) {
    VLOG(1) << "Unable to load ppapi plugin: " << plugin_path.MaybeAsASCII();
    return nullptr;
  }

  // Validate that the plugin is actually registered.
  const PepperPluginInfo* info = GetRegisteredPpapiPluginInfo(plugin_path);
  if (!info) {
    VLOG(1) << "Unable to find ppapi plugin registration for: "
            << plugin_path.MaybeAsASCII();
    return nullptr;
  }

  // Flash has its own flavour of CORS, so CORB needs to allow all responses
  // and rely on Flash to enforce same-origin policy.  See also
  // https://crbug.com/874515 and https://crbug.com/816318#c5.
  //
  // Note that ppapi::PERMISSION_FLASH is present not only in the Flash plugin.
  // This permission is also present in plugins added from the cmdline and so
  // will be also present for "PPAPI Tests" plugin used for
  // OutOfProcessPPAPITest.URLLoaderTrusted and related tests.
  //
  // TODO(lukasza, laforge): https://crbug.com/702995: Remove the code below
  // once Flash support is removed from Chromium (probably around 2020 - see
  // https://www.chromium.org/flash-roadmap).
  if (info->permissions & ppapi::PERMISSION_FLASH)
    RenderProcessHostImpl::AddCorbExceptionForPlugin(render_process_id);

  PpapiPluginProcessHost* plugin_host =
      FindPpapiPluginProcess(plugin_path, profile_data_directory, origin_lock);
  if (plugin_host)
    return plugin_host;

  // Record when PPAPI Flash process is started for the first time.
  static bool counted = false;
  if (!counted && info->name == kFlashPluginName) {
    counted = true;
    UMA_HISTOGRAM_ENUMERATION("Plugin.FlashUsage",
                              START_PPAPI_FLASH_AT_LEAST_ONCE,
                              FLASH_USAGE_ENUM_COUNT);
  }

  // Avoid fork bomb.
  if (origin_lock.has_value() && CountPpapiPluginProcessesForProfile(
                                     plugin_path, profile_data_directory) >=
                                     max_ppapi_processes_per_profile_) {
    return nullptr;
  }

  // This plugin isn't loaded by any plugin process, so create a new process.
  plugin_host = PpapiPluginProcessHost::CreatePluginHost(
      *info, profile_data_directory, origin_lock);
  if (!plugin_host) {
    VLOG(1) << "Unable to create ppapi plugin process for: "
            << plugin_path.MaybeAsASCII();
  }

  return plugin_host;
}

PpapiPluginProcessHost* PluginServiceImpl::FindOrStartPpapiBrokerProcess(
    int render_process_id,
    const base::FilePath& plugin_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (filter_ && !filter_->CanLoadPlugin(render_process_id, plugin_path))
    return nullptr;

  PpapiPluginProcessHost* plugin_host = FindPpapiBrokerProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  // Validate that the plugin is actually registered.
  const PepperPluginInfo* info = GetRegisteredPpapiPluginInfo(plugin_path);
  if (!info)
    return nullptr;

  DCHECK(info->is_out_of_process);

  // This broker isn't loaded by any broker process, so create a new process.
  return PpapiPluginProcessHost::CreateBrokerHost(*info);
}

void PluginServiceImpl::OpenChannelToPpapiPlugin(
    int render_process_id,
    const base::FilePath& plugin_path,
    const base::FilePath& profile_data_directory,
    const base::Optional<url::Origin>& origin_lock,
    PpapiPluginProcessHost::PluginClient* client) {
  PpapiPluginProcessHost* plugin_host = FindOrStartPpapiPluginProcess(
      render_process_id, plugin_path, profile_data_directory, origin_lock);
  if (plugin_host) {
    plugin_host->OpenChannelToPlugin(client);
  } else {
    // Send error.
    client->OnPpapiChannelOpened(IPC::ChannelHandle(), base::kNullProcessId, 0);
  }
}

void PluginServiceImpl::OpenChannelToPpapiBroker(
    int render_process_id,
    int render_frame_id,
    const base::FilePath& path,
    PpapiPluginProcessHost::BrokerClient* client) {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&PluginServiceImpl::RecordBrokerUsage,
                                render_process_id, render_frame_id));

  PpapiPluginProcessHost* plugin_host = FindOrStartPpapiBrokerProcess(
      render_process_id, path);
  if (plugin_host) {
    plugin_host->OpenChannelToPlugin(client);
  } else {
    // Send error.
    client->OnPpapiChannelOpened(IPC::ChannelHandle(), base::kNullProcessId, 0);
  }
}

bool PluginServiceImpl::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    std::vector<WebPluginInfo>* plugins,
    std::vector<std::string>* actual_mime_types) {
  return PluginList::Singleton()->GetPluginInfoArray(
      url, mime_type, allow_wildcard, plugins, actual_mime_types);
}

bool PluginServiceImpl::GetPluginInfo(int render_process_id,
                                      int render_frame_id,
                                      const GURL& url,
                                      const url::Origin& main_frame_origin,
                                      const std::string& mime_type,
                                      bool allow_wildcard,
                                      bool* is_stale,
                                      WebPluginInfo* info,
                                      std::string* actual_mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<WebPluginInfo> plugins;
  std::vector<std::string> mime_types;
  bool stale = GetPluginInfoArray(
      url, mime_type, allow_wildcard, &plugins, &mime_types);
  if (is_stale)
    *is_stale = stale;

  for (size_t i = 0; i < plugins.size(); ++i) {
    if (!filter_ ||
        filter_->IsPluginAvailable(render_process_id, render_frame_id, url,
                                   main_frame_origin, &plugins[i])) {
      *info = plugins[i];
      if (actual_mime_type)
        *actual_mime_type = mime_types[i];
      return true;
    }
  }
  return false;
}

bool PluginServiceImpl::GetPluginInfoByPath(const base::FilePath& plugin_path,
                                            WebPluginInfo* info) {
  std::vector<WebPluginInfo> plugins;
  PluginList::Singleton()->GetPluginsNoRefresh(&plugins);

  for (const WebPluginInfo& plugin : plugins) {
    if (plugin.path == plugin_path) {
      *info = plugin;
      return true;
    }
  }

  return false;
}

base::string16 PluginServiceImpl::GetPluginDisplayNameByPath(
    const base::FilePath& path) {
  base::string16 plugin_name = path.LossyDisplayName();
  WebPluginInfo info;
  if (PluginService::GetInstance()->GetPluginInfoByPath(path, &info) &&
      !info.name.empty()) {
    plugin_name = info.name;
#if defined(OS_MACOSX)
    // Many plugins on the Mac have .plugin in the actual name, which looks
    // terrible, so look for that and strip it off if present.
    static const char kPluginExtension[] = ".plugin";
    if (base::EndsWith(plugin_name, base::ASCIIToUTF16(kPluginExtension),
                       base::CompareCase::SENSITIVE))
      plugin_name.erase(plugin_name.length() - strlen(kPluginExtension));
#endif  // defined(OS_MACOSX)
  }
  return plugin_name;
}

void PluginServiceImpl::GetPlugins(GetPluginsCallback callback) {
  base::PostTaskAndReplyWithResult(
      plugin_list_task_runner_.get(), FROM_HERE, base::BindOnce([]() {
        std::vector<WebPluginInfo> plugins;
        PluginList::Singleton()->GetPlugins(&plugins);
        return plugins;
      }),
      std::move(callback));
}

void PluginServiceImpl::RegisterPepperPlugins() {
  ComputePepperPluginList(&ppapi_plugins_);
  for (const auto& plugin : ppapi_plugins_)
    RegisterInternalPlugin(plugin.ToWebPluginInfo(), /*add_at_beginning=*/true);
}

// There should generally be very few plugins so a brute-force search is fine.
const PepperPluginInfo* PluginServiceImpl::GetRegisteredPpapiPluginInfo(
    const base::FilePath& plugin_path) {
  for (auto& plugin : ppapi_plugins_) {
    if (plugin.path == plugin_path)
      return &plugin;
  }

  // We did not find the plugin in our list. But wait! the plugin can also
  // be a latecomer, as it happens with pepper flash. This information
  // can be obtained from the PluginList singleton and we can use it to
  // construct it and add it to the list. This same deal needs to be done
  // in the renderer side in PepperPluginRegistry.
  WebPluginInfo webplugin_info;
  if (!GetPluginInfoByPath(plugin_path, &webplugin_info))
    return nullptr;
  PepperPluginInfo new_pepper_info;
  if (!MakePepperPluginInfo(webplugin_info, &new_pepper_info))
    return nullptr;
  ppapi_plugins_.push_back(new_pepper_info);
  return &ppapi_plugins_.back();
}

void PluginServiceImpl::SetFilter(PluginServiceFilter* filter) {
  filter_ = filter;
}

PluginServiceFilter* PluginServiceImpl::GetFilter() {
  return filter_;
}

static const unsigned int kMaxCrashesPerInterval = 3;
static const unsigned int kCrashesInterval = 120;

void PluginServiceImpl::RegisterPluginCrash(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto i = crash_times_.find(path);
  if (i == crash_times_.end()) {
    crash_times_[path] = std::vector<base::Time>();
    i = crash_times_.find(path);
  }
  if (i->second.size() == kMaxCrashesPerInterval) {
    i->second.erase(i->second.begin());
  }
  base::Time time = base::Time::Now();
  i->second.push_back(time);
}

bool PluginServiceImpl::IsPluginUnstable(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::map<base::FilePath, std::vector<base::Time> >::const_iterator i =
      crash_times_.find(path);
  if (i == crash_times_.end()) {
    return false;
  }
  if (i->second.size() != kMaxCrashesPerInterval) {
    return false;
  }
  base::TimeDelta delta = base::Time::Now() - i->second[0];
  return delta.InSeconds() <= kCrashesInterval;
}

void PluginServiceImpl::RefreshPlugins() {
  PluginList::Singleton()->RefreshPlugins();
}

void PluginServiceImpl::RegisterInternalPlugin(
    const WebPluginInfo& info,
    bool add_at_beginning) {
  PluginList::Singleton()->RegisterInternalPlugin(info, add_at_beginning);
}

void PluginServiceImpl::UnregisterInternalPlugin(const base::FilePath& path) {
  PluginList::Singleton()->UnregisterInternalPlugin(path);
}

void PluginServiceImpl::GetInternalPlugins(
    std::vector<WebPluginInfo>* plugins) {
  PluginList::Singleton()->GetInternalPlugins(plugins);
}

bool PluginServiceImpl::PpapiDevChannelSupported(
    BrowserContext* browser_context,
    const GURL& document_url) {
  return GetContentClient()->browser()->IsPluginAllowedToUseDevChannelAPIs(
      browser_context, document_url);
}

}  // namespace content
