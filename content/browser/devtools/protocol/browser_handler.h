// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_

#include <map>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/devtools/protocol/browser.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {

class BrowserContext;
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
      const Maybe<std::string>& browser_context_id,
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
      Maybe<std::string> in_query,
      Maybe<bool> in_delta,
      std::unique_ptr<Array<Browser::Histogram>>* histograms) override;

  Response GetHistogram(
      const std::string& in_name,
      Maybe<bool> in_delta,
      std::unique_ptr<Browser::Histogram>* out_histogram) override;

  Response GetBrowserCommandLine(
      std::unique_ptr<protocol::Array<std::string>>* arguments) override;

  Response SetPermission(
      std::unique_ptr<protocol::Browser::PermissionDescriptor> permission,
      const protocol::Browser::PermissionSetting& setting,
      Maybe<std::string> origin,
      Maybe<std::string> browser_context_id) override;

  Response GrantPermissions(
      std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
          permissions,
      Maybe<std::string> origin,
      Maybe<std::string> browser_context_id) override;

  Response ResetPermissions(Maybe<std::string> browser_context_id) override;

  Response SetDownloadBehavior(const std::string& behavior,
                               Maybe<std::string> browser_context_id,
                               Maybe<std::string> download_path,
                               Maybe<bool> events_enabled) override;
  Response DoSetDownloadBehavior(const std::string& behavior,
                                 BrowserContext* browser_context,
                                 Maybe<std::string> download_path);

  Response CancelDownload(const std::string& guid,
                          Maybe<std::string> browser_context_id) override;

  Response Crash() override;
  Response CrashGpuProcess() override;

  // DownloadItem::Observer overrides
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

  void DownloadWillBegin(FrameTreeNode* ftn, download::DownloadItem* item);

 private:
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
  std::map<std::string, std::unique_ptr<base::HistogramSamples>>
      histograms_snapshots_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
