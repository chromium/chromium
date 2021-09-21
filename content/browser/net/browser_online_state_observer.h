// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_
#define CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/network_change_notifier.h"

namespace content {

// Listens for changes to the online state and manages sending
// updates to each RenderProcess via RenderProcessHost IPC.
class BrowserOnlineStateObserver
    : public net::NetworkChangeNotifier::MaxBandwidthObserver,
      public content::NotificationObserver {
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

  // NotificationObserver implementation
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  content::NotificationRegistrar registrar_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_BROWSER_ONLINE_STATE_OBSERVER_H_
