// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_ANDROID_H_
#define NET_DNS_DNS_CONFIG_SERVICE_ANDROID_H_

#include <memory>

#include "base/time/time.h"
#include "net/android/network_library.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_config_service.h"

namespace net {

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

// Service for reading and watching Android system DNS settings. This object is
// not thread-safe and methods may perform blocking I/O so methods must be
// called on a sequence that allows blocking (i.e. base::MayBlock). It may be
// constructed on a different sequence than which it's later called on.
class NET_EXPORT_PRIVATE DnsConfigServiceAndroid
    : public DnsConfigService,
      public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  static constexpr base::TimeDelta kConfigChangeDelay = base::Milliseconds(50);

  DnsConfigServiceAndroid();
  ~DnsConfigServiceAndroid() override;

  DnsConfigServiceAndroid(const DnsConfigServiceAndroid&) = delete;
  DnsConfigServiceAndroid& operator=(const DnsConfigServiceAndroid&) = delete;

  // To be effective, must be called before the first config read. Also, may
  // outlive `this` and be run on other sequences.
  void set_dns_server_getter_for_testing(
      android::DnsServerGetter dns_server_getter) {
    dns_server_getter_ = std::move(dns_server_getter);
  }

 protected:
  // DnsConfigService:
  void ReadConfigNow() override;
  bool StartWatching() override;

 private:
  class ConfigReader;

  // NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) override;

  bool is_watching_network_change_ = false;
  std::unique_ptr<ConfigReader> config_reader_;
  android::DnsServerGetter dns_server_getter_;
};

}  // namespace internal
}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_ANDROID_H_
