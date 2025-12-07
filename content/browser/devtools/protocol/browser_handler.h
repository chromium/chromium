// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_

#include <map>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/types/optional_ref.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/devtools/protocol/browser.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {

class BrowserContext;
class DevToolsAgentHostImpl;
class FrameTreeNode;

namespace protocol {

class BrowserHandler : public DevToolsDomainHandler,
                       public Browser::Backend,
                       public download::DownloadItem::Observer {
 public:
  explicit BrowserHandler(bool allow_set_download_behavior);

  BrowserHandler(const BrowserHandler&) = delete;
  BrowserHandler& operator=(const BrowserHandler&) = delete;

  ~BrowserHandler() override;

  static Response FindBrowserContext(
      const std::optional<std::string>& browser_context_id,
      BrowserContext** browser_context);

  static std::vector<BrowserHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;

  Response Disable() override;

  // Protocol methods.
  Response GetVersion(std::string* protocol_version,
                      std::string* product,
                      std::string* revision,
                      std::string* user_agent,
                      std::string* js_version) override;

  Response GetHistograms(
      std::optional<std::string> in_query,
      std::optional<bool> in_delta,
      std::unique_ptr<Array<Browser::Histogram>>* histograms) override;

  Response GetHistogram(
      const std::string& in_name,
      std::optional<bool> in_delta,
      std::unique_ptr<Browser::Histogram>* out_histogram) override;

  Response GetBrowserCommandLine(
      std::unique_ptr<protocol::Array<std::string>>* arguments) override;

  void SetPermission(
      std::unique_ptr<protocol::Browser::PermissionDescriptor> permission,
      const protocol::Browser::PermissionSetting& setting,
      std::optional<std::string> origin,
      std::optional<std::string> embedded_origin,
      std::optional<std::string> browser_context_id,
      std::unique_ptr<protocol::Browser::Backend::SetPermissionCallback>
          callback) override;

  void GrantPermissions(
      std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
          permissions,
      std::optional<std::string> origin,
      std::optional<std::string> browser_context_id,
      std::unique_ptr<protocol::Browser::Backend::GrantPermissionsCallback>
          callback) override;

  void ResetPermissions(
      std::optional<std::string> browser_context_id,
      std::unique_ptr<protocol::Browser::Backend::ResetPermissionsCallback>
          callback) override;

  Response SetDownloadBehavior(const std::string& behavior,
                               std::optional<std::string> browser_context_id,
                               std::optional<std::string> download_path,
                               std::optional<bool> events_enabled) override;
  Response DoSetDownloadBehavior(const std::string& behavior,
                                 BrowserContext* browser_context,
                                 std::optional<std::string> download_path);

  Response CancelDownload(
      const std::string& guid,
      std::optional<std::string> browser_context_id) override;

  Response Crash() override;
  Response CrashGpuProcess() override;

  void AddPrivacySandboxCoordinatorKeyConfig(
      const std::string& in_api,
      const std::string& in_coordinator_origin,
      const std::string& in_key_config,
      std::optional<std::string> browser_context_id,
      std::unique_ptr<AddPrivacySandboxCoordinatorKeyConfigCallback> callback)
      override;

  // DownloadItem::Observer overrides
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

  void DownloadWillBegin(FrameTreeNode* ftn, download::DownloadItem* item);

 private:
  // Adds the `browser_context_id` to contexts_with_overridden_permissions_.
  void UpdateContextsWithOverriddenPermissions(
      base::optional_ref<const std::string> browser_context_id);

  void SetDownloadEventsEnabled(bool enabled);

  // Retrieves the data for the given histogram, returning it in the converted
  // format. If `get_delta` is true, returns the only the new data since the
  // last `get_delta` true call for the given histogram, or all data if it's
  // the first such call.
  std::unique_ptr<Browser::Histogram> GetHistogramData(
      const base::HistogramBase& histogram,
      bool get_delta);

  std::unique_ptr<Browser::Frontend> frontend_;
  base::flat_set<std::string> contexts_with_overridden_permissions_;
  base::flat_set<std::string> contexts_with_overridden_downloads_;
  bool download_events_enabled_;
  const bool allow_set_download_behavior_;
  base::flat_set<raw_ptr<download::DownloadItem, CtnExperimental>>
      pending_downloads_;
  // Stores past histogram snapshots for producing histogram deltas.
  std::map<std::string, std::unique_ptr<base::HistogramSamples>, std::less<>>
      histograms_snapshots_;

  base::WeakPtrFactory<BrowserHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
