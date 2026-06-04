// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_DECLARATIVE_PERFORMANCE_OBSERVER_H_
#define CONTENT_BROWSER_NETWORK_DECLARATIVE_PERFORMANCE_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/declarative_performance_observer.mojom.h"
#include "third_party/blink/public/mojom/timing/declarative_performance_observer.mojom.h"
#include "url/gurl.h"

namespace content {

class CONTENT_EXPORT DeclarativePerformanceObserver
    : public WebContentsObserver,
      public WebContentsUserData<DeclarativePerformanceObserver>,
      public blink::mojom::DeclarativePerformanceObserverHost {
 public:
  explicit DeclarativePerformanceObserver(WebContents* web_contents);
  ~DeclarativePerformanceObserver() override;

  DeclarativePerformanceObserver(const DeclarativePerformanceObserver&) =
      delete;
  DeclarativePerformanceObserver& operator=(
      const DeclarativePerformanceObserver&) = delete;

  // WebContentsObserver overrides:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(Visibility visibility) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;

  // blink::mojom::DeclarativePerformanceObserverHost:
  void DidObservePerformanceEntries(
      std::vector<blink::mojom::DeclarativePerformanceEntryPtr> entries)
      override;

  static void Bind(
      RenderFrameHost* rfh,
      mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
          receiver);

  void SetStoragePartitionForTesting(  // IN-TEST
      StoragePartition* storage_partition);

 private:
  friend class WebContentsUserData<DeclarativePerformanceObserver>;

  void BindReceiver(
      RenderFrameHost* rfh,
      mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
          receiver);

  void AddEntryToBuffer(base::DictValue entry);
  void FlushMetrics(RenderFrameHost* rfh);
  void AppendSessionEndEntry();

  std::string reporting_endpoint_;
  base::flat_set<network::mojom::PerformanceEntryType> enabled_types_;
  std::optional<base::flat_set<std::string>> include_user_timing_;
  base::ListValue buffered_entries_;
  bool started_in_foreground_ = false;
  base::TimeTicks navigation_start_;
  GURL committed_url_;
  net::NetworkAnonymizationKey network_anonymization_key_;
  base::UnguessableToken reporting_source_;
  GlobalRenderFrameHostId active_rfh_;
  raw_ptr<StoragePartition> storage_partition_for_testing_ = nullptr;

  mojo::ReceiverSet<blink::mojom::DeclarativePerformanceObserverHost,
                    GlobalRenderFrameHostId>
      receivers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_DECLARATIVE_PERFORMANCE_OBSERVER_H_
