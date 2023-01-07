// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_BROWSER_ONLINE_STATE_OBSERVER_H_
#define CONTENT_BROWSER_NETWORK_BROWSER_ONLINE_STATE_OBSERVER_H_

#include "content/public/browser/render_process_host_creation_observer.h"
#include "net/base/network_change_notifier.h"

namespace content {

// Listens for changes to the online state and manages sending
// updates to each RenderProcess via RenderProcessHost IPC.
class BrowserOnlineStateObserver
    : public net::NetworkChangeNotifier::MaxBandwidthObserver,
      public content::RenderProcessHostCreationObserver {
 public:
  BrowserOnlineStateObserver();

  BrowserOnlineStateObserver(const BrowserOnlineStateObserver&) = delete;
  BrowserOnlineStateObserver& operator=(const BrowserOnlineStateObserver&) =
      delete;

  ~BrowserOnlineStateObserver() override;

  // MaxBandwidthObserver implementation
  void OnMaxBandwidthChanged(
      double max_bandwidth_mbps,
      net::NetworkChangeNotifier::ConnectionType type) override;

  // content::RenderProcessHostCreationObserver implementation
  void OnRenderProcessHostCreated(content::RenderProcessHost* rph) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_BROWSER_ONLINE_STATE_OBSERVER_H_
