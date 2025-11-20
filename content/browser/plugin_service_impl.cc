// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service_impl.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/plugin_list.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/content_switches_internal.h"
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
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

// static
PluginService* PluginService::GetInstance() {
  return PluginServiceImpl::GetInstance();
}

void PluginService::PurgePluginListCache(BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (!browser_context || host->GetBrowserContext() == browser_context)
      host->GetRendererInterface()->PurgePluginListCache();
  }
}

// static
PluginServiceImpl* PluginServiceImpl::GetInstance() {
  return base::Singleton<PluginServiceImpl>::get();
}

PluginServiceImpl::PluginServiceImpl() = default;

PluginServiceImpl::~PluginServiceImpl() = default;

void PluginServiceImpl::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RegisterPlugins();
}

void PluginServiceImpl::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    std::vector<WebPluginInfo>* plugins,
    std::vector<std::string>* actual_mime_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PluginList::Singleton()->GetPluginInfoArray(url, mime_type, plugins,
                                              actual_mime_types);
}

bool PluginServiceImpl::GetPluginInfo(content::BrowserContext* browser_context,
                                      const GURL& url,
                                      const std::string& mime_type,
                                      WebPluginInfo* info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<WebPluginInfo> plugins;
  GetPluginInfoArray(url, mime_type, &plugins, /*actual_mime_types=*/nullptr);

  for (size_t i = 0; i < plugins.size(); ++i) {
    if (!filter_ || filter_->IsPluginAvailable(browser_context, plugins[i])) {
      *info = plugins[i];
      return true;
    }
  }
  return false;
}

std::optional<WebPluginInfo> PluginServiceImpl::GetPluginInfoByPathForTesting(
    const base::FilePath& plugin_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const WebPluginInfo& plugin :
       PluginList::Singleton()->GetPluginsForTesting()) {
    if (plugin.path == plugin_path) {
      return plugin;
    }
  }

  return std::nullopt;
}

void PluginServiceImpl::GetPluginsAsync(GetPluginsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Run `callback` later, to stay compatible with prior behavior.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetPlugins()));
}

const std::vector<WebPluginInfo>& PluginServiceImpl::GetPlugins() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return PluginList::Singleton()->GetPlugins();
}

void PluginServiceImpl::RegisterPlugins() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetContentClient()->AddPlugins(&plugins_);
  for (const auto& plugin : plugins_) {
    RegisterInternalPlugin(plugin);
  }
}

void PluginServiceImpl::SetFilter(PluginServiceFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  filter_ = filter;
}

PluginServiceFilter* PluginServiceImpl::GetFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return filter_;
}

void PluginServiceImpl::RegisterInternalPlugin(const WebPluginInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PluginList::Singleton()->RegisterInternalPlugin(info);
}

void PluginServiceImpl::UnregisterInternalPlugin(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PluginList::Singleton()->UnregisterInternalPlugin(path);
}

std::vector<WebPluginInfo> PluginServiceImpl::GetInternalPluginsForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return PluginList::Singleton()->GetInternalPluginsForTesting();
}

}  // namespace content
