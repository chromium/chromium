// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_android.h"

#include <sys/system_properties.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/android/android_info.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/android/network_library.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_interfaces.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/serial_worker.h"

namespace net {
namespace internal {

namespace {

constexpr base::FilePath::CharType kFilePathHosts[] =
    FILE_PATH_LITERAL("/system/etc/hosts");

}  // namespace

// static
constexpr base::TimeDelta DnsConfigServiceAndroid::kConfigChangeDelay;

class DnsConfigServiceAndroid::ConfigReader : public SerialWorker {
 public:
  explicit ConfigReader(DnsConfigServiceAndroid& service,
                        android::DnsServerGetter dns_server_getter)
      : dns_server_getter_(std::move(dns_server_getter)), service_(&service) {}

  ~ConfigReader() override = default;

  ConfigReader(const ConfigReader&) = delete;
  ConfigReader& operator=(const ConfigReader&) = delete;

  std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override {
    return std::make_unique<WorkItem>(dns_server_getter_);
  }

  bool OnWorkFinished(std::unique_ptr<SerialWorker::WorkItem>
                          serial_worker_work_item) override {
    DCHECK(serial_worker_work_item);
    DCHECK(!IsCancelled());

    WorkItem* work_item = static_cast<WorkItem*>(serial_worker_work_item.get());
    if (work_item->dns_config_.has_value()) {
      service_->OnConfigRead(std::move(work_item->dns_config_).value());
      return true;
    } else {
      LOG(WARNING) << "Failed to read DnsConfig.";
      return false;
    }
  }

 private:
  class WorkItem : public SerialWorker::WorkItem {
   public:
    explicit WorkItem(android::DnsServerGetter dns_server_getter)
        : dns_server_getter_(std::move(dns_server_getter)) {}

    void DoWork() override {
      dns_config_.emplace();
      dns_config_->unhandled_options = false;

      if (!dns_server_getter_.Run(
              &dns_config_->nameservers, &dns_config_->dns_over_tls_active,
              &dns_config_->dns_over_tls_hostname, &dns_config_->search)) {
        dns_config_.reset();
      }
    }

   private:
    friend class ConfigReader;
    android::DnsServerGetter dns_server_getter_;
    std::optional<DnsConfig> dns_config_;
  };

  android::DnsServerGetter dns_server_getter_;

  // Raw pointer to owning DnsConfigService.
  const raw_ptr<DnsConfigServiceAndroid> service_;
};

DnsConfigServiceAndroid::DnsConfigServiceAndroid()
    : DnsConfigService(kFilePathHosts, kConfigChangeDelay) {
  // Allow constructing on one thread and living on another.
  DETACH_FROM_SEQUENCE(sequence_checker_);
  dns_server_getter_ = base::BindRepeating(&android::GetCurrentDnsServers);
}

DnsConfigServiceAndroid::~DnsConfigServiceAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_watching_network_change_) {
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }
  if (config_reader_)
    config_reader_->Cancel();
}

void DnsConfigServiceAndroid::ReadConfigNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!config_reader_) {
    DCHECK(dns_server_getter_);
    config_reader_ =
        std::make_unique<ConfigReader>(*this, std::move(dns_server_getter_));
  }
  config_reader_->WorkNow();
}

bool DnsConfigServiceAndroid::StartWatching() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_watching_network_change_);
  is_watching_network_change_ = true;

  // On Android, assume DNS config may have changed on every network change.
  NetworkChangeNotifier::AddNetworkChangeObserver(this);

  // Hosts file should never change on Android (and watching it is
  // problematic; see http://crbug.com/600442), so don't watch it.

  return true;
}

void DnsConfigServiceAndroid::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type != NetworkChangeNotifier::CONNECTION_NONE) {
    OnConfigChanged(/*succeeded=*/true);
  }
}
}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return std::make_unique<internal::DnsConfigServiceAndroid>();
}

}  // namespace net
