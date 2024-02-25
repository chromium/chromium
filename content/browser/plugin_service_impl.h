// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_

#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/plugin_service.h"
#include "ppapi/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/browser/ppapi_plugin_process_host.h"
#endif

namespace content {
class PluginServiceFilter;
struct ContentPluginInfo;

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances. It lives on the UI thread.
class CONTENT_EXPORT PluginServiceImpl : public PluginService {
 public:
  // Returns the PluginServiceImpl singleton.
  static PluginServiceImpl* GetInstance();

  PluginServiceImpl(const PluginServiceImpl&) = delete;
  PluginServiceImpl& operator=(const PluginServiceImpl&) = delete;

  // PluginService implementation:
  void Init() override;
  bool GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types) override;
  bool GetPluginInfo(content::BrowserContext* browser_context,
                     const GURL& url,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     WebPluginInfo* info,
                     std::string* actual_mime_type) override;
  bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                           WebPluginInfo* info) override;
  std::u16string GetPluginDisplayNameByPath(
      const base::FilePath& path) override;
  void GetPlugins(GetPluginsCallback callback) override;
  std::vector<WebPluginInfo> GetPluginsSynchronous() override;
  const ContentPluginInfo* GetRegisteredPluginInfo(
      const base::FilePath& plugin_path) override;
  void SetFilter(PluginServiceFilter* filter) override;
  PluginServiceFilter* GetFilter() override;
  bool IsPluginUnstable(const base::FilePath& plugin_path) override;
  void RefreshPlugins() override;
  void RegisterInternalPlugin(const WebPluginInfo& info,
                              bool add_at_beginning) override;
  void UnregisterInternalPlugin(const base::FilePath& path) override;
  void GetInternalPlugins(std::vector<WebPluginInfo>* plugins) override;
  bool PpapiDevChannelSupported(BrowserContext* browser_context,
                                const GURL& document_url) override;

#if BUILDFLAG(ENABLE_PPAPI)
  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL.
  PpapiPluginProcessHost* FindOrStartPpapiPluginProcess(
      int render_process_id,
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory,
      const std::optional<url::Origin>& origin_lock);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.
  void OpenChannelToPpapiPlugin(int render_process_id,
                                const base::FilePath& plugin_path,
                                const base::FilePath& profile_data_directory,
                                const std::optional<url::Origin>& origin_lock,
                                PpapiPluginProcessHost::PluginClient* client);
#endif  // BUILDFLAG(ENABLE_PPAPI)

  // Used to monitor plugin stability.
  void RegisterPluginCrash(const base::FilePath& plugin_path);

  // For testing without creating many, many processes.
  void SetMaxPpapiProcessesPerProfileForTesting(int number) {
    max_ppapi_processes_per_profile_ = number;
  }

 private:
  friend struct base::DefaultSingletonTraits<PluginServiceImpl>;

  // Pulled out of the air, seems reasonable.
  static constexpr int kDefaultMaxPpapiProcessesPerProfile = 15;

  // Creates the PluginServiceImpl object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginServiceImpl();
  ~PluginServiceImpl() override;

#if BUILDFLAG(ENABLE_PPAPI)
  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PpapiPluginProcessHost* FindPpapiPluginProcess(
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory,
      const std::optional<url::Origin>& origin_lock);
#endif  // BUILDFLAG(ENABLE_PPAPI)

  void RegisterPlugins();

  std::vector<ContentPluginInfo> plugins_;

  int max_ppapi_processes_per_profile_ = kDefaultMaxPpapiProcessesPerProfile;

  // Weak pointer; set during the startup and must outlive us.
  raw_ptr<PluginServiceFilter, DanglingUntriaged> filter_ = nullptr;

  // Used to detect if a given plugin is crashing over and over.
  std::map<base::FilePath, std::vector<base::Time>> crash_times_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
