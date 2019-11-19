// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_

#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ipc/ipc_channel_handle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class PluginServiceFilter;
struct PepperPluginInfo;

class CONTENT_EXPORT PluginServiceImpl : public PluginService {
 public:
  // Returns the PluginServiceImpl singleton.
  static PluginServiceImpl* GetInstance();

  // PluginService implementation:
  void Init() override;
  bool GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types) override;
  bool GetPluginInfo(int render_process_id,
                     int render_frame_id,
                     const GURL& url,
                     const url::Origin& main_frame_origin,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     WebPluginInfo* info,
                     std::string* actual_mime_type) override;
  bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                           WebPluginInfo* info) override;
  base::string16 GetPluginDisplayNameByPath(
      const base::FilePath& path) override;
  void GetPlugins(GetPluginsCallback callback) override;
  const PepperPluginInfo* GetRegisteredPpapiPluginInfo(
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
  int CountPpapiPluginProcessesForProfile(
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory) override;

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL. Must be called on the IO thread.
  PpapiPluginProcessHost* FindOrStartPpapiPluginProcess(
      int render_process_id,
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory,
      const base::Optional<url::Origin>& origin_lock);
  PpapiPluginProcessHost* FindOrStartPpapiBrokerProcess(
      int render_process_id, const base::FilePath& plugin_path);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.  This must be called on the IO thread
  // or else a deadlock can occur.
  void OpenChannelToPpapiPlugin(int render_process_id,
                                const base::FilePath& plugin_path,
                                const base::FilePath& profile_data_directory,
                                const base::Optional<url::Origin>& origin_lock,
                                PpapiPluginProcessHost::PluginClient* client);
  void OpenChannelToPpapiBroker(int render_process_id,
                                int render_frame_id,
                                const base::FilePath& path,
                                PpapiPluginProcessHost::BrokerClient* client);

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

  // Helper for recording URLs to UKM.
  static void RecordBrokerUsage(int render_process_id, int render_frame_id);

  // Creates the PluginServiceImpl object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginServiceImpl();
  ~PluginServiceImpl() override;

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PpapiPluginProcessHost* FindPpapiPluginProcess(
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory,
      const base::Optional<url::Origin>& origin_lock);
  PpapiPluginProcessHost* FindPpapiBrokerProcess(
      const base::FilePath& broker_path);

  void RegisterPepperPlugins();

  std::vector<PepperPluginInfo> ppapi_plugins_;

  int max_ppapi_processes_per_profile_ = kDefaultMaxPpapiProcessesPerProfile;

  // Weak pointer; set during the startup on UI thread and must outlive us.
  PluginServiceFilter* filter_;

  // Used to load plugins from disk.
  scoped_refptr<base::SequencedTaskRunner> plugin_list_task_runner_;

  // Used to verify that loading plugins from disk is done sequentially.
  base::SequenceChecker plugin_list_sequence_checker_;

  // Used to detect if a given plugin is crashing over and over.
  std::map<base::FilePath, std::vector<base::Time> > crash_times_;

  DISALLOW_COPY_AND_ASSIGN(PluginServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
