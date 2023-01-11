// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_WATCHER_MAC_H_
#define NET_DNS_DNS_CONFIG_WATCHER_MAC_H_

#include "base/functional/callback_forward.h"
#include "net/dns/notify_watcher_mac.h"

namespace net::internal {

// Watches DNS configuration on Mac.
class DnsConfigWatcher {
 public:
  bool Watch(const base::RepeatingCallback<void(bool succeeded)>& callback);

  // Returns false iff a valid config could not be determined.
  static bool CheckDnsConfig(bool& out_unhandled_options);

 private:
  NotifyWatcherMac watcher_;
};

}  // namespace net::internal

#endif  // NET_DNS_DNS_CONFIG_WATCHER_MAC_H_
