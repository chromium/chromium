// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_TEST_DNS_CONFIG_SERVICE_H_
#define NET_DNS_TEST_DNS_CONFIG_SERVICE_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {

class DnsHostsParser;

// Simple test implementation of DnsConfigService that will trigger
// notifications only on explicitly calling On...() methods.
class NET_EXPORT_PRIVATE TestDnsConfigService : public DnsConfigService {
 public:
  TestDnsConfigService();
  ~TestDnsConfigService() override;

  void ReadConfigNow() override {}
  void ReadHostsNow() override {}
  bool StartWatching() override;

  // Expose the protected methods to this test suite.
  void InvalidateConfig() { DnsConfigService::InvalidateConfig(); }

  void InvalidateHosts() { DnsConfigService::InvalidateHosts(); }

  void OnConfigRead(const DnsConfig& config) {
    DnsConfigService::OnConfigRead(config);
  }

  void OnHostsRead(const DnsHosts& hosts) {
    DnsConfigService::OnHostsRead(hosts);
  }

  void RefreshConfig() override;

  void SetConfigForRefresh(DnsConfig config) {
    DCHECK(!config_for_refresh_);
    config_for_refresh_ = std::move(config);
  }

 private:
  std::optional<DnsConfig> config_for_refresh_;
};

// Test implementation of `DnsConfigService` that exercises the
// `DnsConfigService::HostsReader`. Uses an injected `DnsHostsParser`. `Watcher`
// change notifications are simulated using `TriggerHostsChangeNotification()`.
class NET_EXPORT_PRIVATE HostsReadingTestDnsConfigService
    : public TestDnsConfigService {
 public:
  using HostsParserFactory =
      base::RepeatingCallback<std::unique_ptr<DnsHostsParser>(void)>;

  explicit HostsReadingTestDnsConfigService(
      HostsParserFactory hosts_parser_factory);
  ~HostsReadingTestDnsConfigService() override;

  // Simulate a `Watcher` change notification for the HOSTS file.
  void TriggerHostsChangeNotification(bool success) {
    watcher_->TriggerHostsChangeNotification(success);
  }

  // DnsConfigService:
  void ReadHostsNow() override;
  bool StartWatching() override;

 private:
  class HostsReader : public DnsConfigService::HostsReader {
   public:
    HostsReader(TestDnsConfigService& service,
                HostsParserFactory hosts_parser_factory);
    ~HostsReader() override;

    // DnsConfigService::HostsReader:
    std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override;

   private:
    HostsParserFactory hosts_parser_factory_;
  };

  class NET_EXPORT_PRIVATE Watcher : public DnsConfigService::Watcher {
   public:
    explicit Watcher(DnsConfigService& service);
    ~Watcher() override;

    void TriggerHostsChangeNotification(bool success);

    // DnsConfigService::Watcher:
    bool Watch() override;

   private:
    bool watch_started_ = false;
  };

  std::unique_ptr<HostsReader> hosts_reader_;
  std::unique_ptr<Watcher> watcher_ = std::make_unique<Watcher>(*this);
};

}  // namespace net

#endif  // NET_DNS_TEST_DNS_CONFIG_SERVICE_H_
