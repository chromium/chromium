// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOGGING_NETWORK_CHANGE_OBSERVER_H_
#define NET_BASE_LOGGING_NETWORK_CHANGE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_with_source.h"

namespace net {

class NetLog;

// A class that adds NetLog events for network change events coming from the
// net::NetworkChangeNotifier.
class NET_EXPORT LoggingNetworkChangeObserver
    : public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::ConnectionTypeObserver,
      public NetworkChangeNotifier::NetworkChangeObserver,
      public NetworkChangeNotifier::NetworkObserver {
 public:
  // Note: |net_log| must remain valid throughout the lifetime of this
  // LoggingNetworkChangeObserver.
  explicit LoggingNetworkChangeObserver(NetLog* net_log);
  LoggingNetworkChangeObserver(const LoggingNetworkChangeObserver&) = delete;
  LoggingNetworkChangeObserver& operator=(const LoggingNetworkChangeObserver&) =
      delete;
  ~LoggingNetworkChangeObserver() override;

 private:
  // NetworkChangeNotifier::IPAddressObserver implementation.
  void OnIPAddressChanged() override;

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) override;

  // NetworkChangeNotifier::NetworkObserver implementation.
  void OnNetworkConnected(handles::NetworkHandle network) override;
  void OnNetworkDisconnected(handles::NetworkHandle network) override;
  void OnNetworkSoonToDisconnect(handles::NetworkHandle network) override;
  void OnNetworkMadeDefault(handles::NetworkHandle network) override;

  NetLogWithSource net_log_;
};

}  // namespace net

#endif  // NET_BASE_LOGGING_NETWORK_CHANGE_OBSERVER_H_
