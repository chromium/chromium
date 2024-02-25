// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/test_dns_config_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "net/dns/dns_hosts.h"

namespace net {

TestDnsConfigService::TestDnsConfigService()
    : DnsConfigService(base::FilePath::StringPieceType() /* hosts_file_path */,
                       std::nullopt /* config_change_delay */) {}

TestDnsConfigService::~TestDnsConfigService() = default;

bool TestDnsConfigService::StartWatching() {
  return true;
}

void TestDnsConfigService::RefreshConfig() {
  DCHECK(config_for_refresh_);
  InvalidateConfig();
  InvalidateHosts();
  OnConfigRead(config_for_refresh_.value());
  OnHostsRead(config_for_refresh_.value().hosts);
  config_for_refresh_ = std::nullopt;
}

HostsReadingTestDnsConfigService::HostsReadingTestDnsConfigService(
    HostsParserFactory hosts_parser_factory)
    : hosts_reader_(
          std::make_unique<HostsReader>(*this,
                                        std::move(hosts_parser_factory))) {}

HostsReadingTestDnsConfigService::~HostsReadingTestDnsConfigService() = default;

void HostsReadingTestDnsConfigService::ReadHostsNow() {
  hosts_reader_->WorkNow();
}

bool HostsReadingTestDnsConfigService::StartWatching() {
  watcher_->Watch();
  return true;
}

HostsReadingTestDnsConfigService::HostsReader::HostsReader(
    TestDnsConfigService& service,
    HostsParserFactory hosts_parser_factory)
    : DnsConfigService::HostsReader(
          /*hosts_file_path=*/base::FilePath::StringPieceType(),
          service),
      hosts_parser_factory_(std::move(hosts_parser_factory)) {}

HostsReadingTestDnsConfigService::HostsReader::~HostsReader() = default;

std::unique_ptr<SerialWorker::WorkItem>
HostsReadingTestDnsConfigService::HostsReader::CreateWorkItem() {
  return std::make_unique<WorkItem>(hosts_parser_factory_.Run());
}

HostsReadingTestDnsConfigService::Watcher::Watcher(DnsConfigService& service)
    : DnsConfigService::Watcher(service) {}

HostsReadingTestDnsConfigService::Watcher::~Watcher() = default;

void HostsReadingTestDnsConfigService::Watcher::TriggerHostsChangeNotification(
    bool success) {
  CHECK(watch_started_);
  OnHostsChanged(success);
}

bool HostsReadingTestDnsConfigService::Watcher::Watch() {
  watch_started_ = true;
  return true;
}

}  // namespace net
