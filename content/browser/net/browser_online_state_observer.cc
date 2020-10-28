// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/browser_online_state_observer.h"

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace content {

BrowserOnlineStateObserver::BrowserOnlineStateObserver() {
  net::NetworkChangeNotifier::AddMaxBandwidthObserver(this);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

BrowserOnlineStateObserver::~BrowserOnlineStateObserver() {
  net::NetworkChangeNotifier::RemoveMaxBandwidthObserver(this);
}

void BrowserOnlineStateObserver::OnMaxBandwidthChanged(
    double max_bandwidth_mbps,
    net::NetworkChangeNotifier::ConnectionType type) {
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    // TODO(https://crbug.com/813045): Remove this check once we have a better
    // way of iterating the hosts.
    if (it.GetCurrentValue()->IsInitializedAndNotDead()) {
      it.GetCurrentValue()->GetRendererInterface()->OnNetworkConnectionChanged(
          type, max_bandwidth_mbps);
    }
  }
}

void BrowserOnlineStateObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(NOTIFICATION_RENDERER_PROCESS_CREATED, type);

  content::RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  double max_bandwidth_mbps;
  net::NetworkChangeNotifier::ConnectionType connection_type;
  net::NetworkChangeNotifier::GetMaxBandwidthAndConnectionType(
      &max_bandwidth_mbps, &connection_type);
  rph->GetRendererInterface()->OnNetworkConnectionChanged(
      connection_type, max_bandwidth_mbps);
}

}  // namespace content
