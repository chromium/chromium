// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/serial_worker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// static
const base::TimeDelta DnsConfigService::kInvalidationTimeout =
    base::Milliseconds(150);

DnsConfigService::DnsConfigService(
    base::FilePath::StringPieceType hosts_file_path,
    absl::optional<base::TimeDelta> config_change_delay)
    : watch_failed_(false),
      have_config_(false),
      have_hosts_(false),
      need_update_(false),
      last_sent_empty_(true),
      config_change_delay_(config_change_delay),
      hosts_file_path_(hosts_file_path) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DnsConfigService::~DnsConfigService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (hosts_reader_)
    hosts_reader_->Cancel();
}

void DnsConfigService::ReadConfig(const CallbackType& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());
  callback_ = callback;
  ReadConfigNow();
  ReadHostsNow();
}

void DnsConfigService::WatchConfig(const CallbackType& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());
  callback_ = callback;
  watch_failed_ = !StartWatching();
  ReadConfigNow();
  ReadHostsNow();
}

void DnsConfigService::RefreshConfig() {
  // Overridden on supported platforms.
  NOTREACHED();
}

DnsConfigService::Watcher::Watcher(DnsConfigService& service)
    : service_(&service) {}

DnsConfigService::Watcher::~Watcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DnsConfigService::Watcher::OnConfigChanged(bool succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_->OnConfigChanged(succeeded);
}

void DnsConfigService::Watcher::OnHostsChanged(bool succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_->OnHostsChanged(succeeded);
}

void DnsConfigService::Watcher::CheckOnCorrectSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DnsConfigService::HostsReader::HostsReader(
    base::FilePath::StringPieceType hosts_file_path,
    DnsConfigService& service)
    : service_(&service), hosts_file_path_(hosts_file_path) {}

DnsConfigService::HostsReader::~HostsReader() = default;

DnsConfigService::HostsReader::WorkItem::WorkItem(
    std::unique_ptr<DnsHostsParser> dns_hosts_parser,
    std::unique_ptr<AddressSorter> address_sorter)
    : dns_hosts_parser_(std::move(dns_hosts_parser)),
      address_sorter_(std::move(address_sorter)) {
  DCHECK(dns_hosts_parser_);
  DCHECK(address_sorter_);
}

DnsConfigService::HostsReader::WorkItem::~WorkItem() = default;

absl::optional<DnsHosts> DnsConfigService::HostsReader::WorkItem::ReadHosts() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DnsHosts dns_hosts;
  if (!dns_hosts_parser_->ParseHosts(&dns_hosts))
    return absl::nullopt;

  return dns_hosts;
}

bool DnsConfigService::HostsReader::WorkItem::AddAdditionalHostsTo(
    DnsHosts& in_out_dns_hosts) {
  // Nothing to add in base implementation.
  return true;
}

void DnsConfigService::HostsReader::WorkItem::DoWork() {
  hosts_ = ReadHosts();
  if (!hosts_.has_value())
    return;

  if (!AddAdditionalHostsTo(hosts_.value()))
    hosts_.reset();
}

void DnsConfigService::HostsReader::WorkItem::FollowupWork(
    base::OnceClosure closure) {
  if (!hosts_.has_value()) {
    std::move(closure).Run();
    return;
  }

  std::vector<const DnsHosts::value_type*> to_sort;
  for (const auto& host : hosts_.value()) {
    const std::vector<IPAddress>& addresses = host.second;
    if (addresses.size() >= 2) {
      to_sort.push_back(&host);
    }
  }

  // This `sort_barrier` is called when each individual sort operation
  // completes. It accumulates all of the inputs it is given into a vector. When
  // it is called for the last time (after `to_sort.size()` calls), it invokes
  // `OnAddressSortComplete()` with that vector of inputs. Should immediately
  // trigger `OnAddressSortComplete()` if `to_sort` is empty.
  SortBarrier sort_barrier =
      base::BarrierCallback<std::pair<DnsHostsKey, AddressList>>(
          to_sort.size(),
          base::BindOnce(&WorkItem::OnAddressSortComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(closure)));

  for (const auto* sort_entry : to_sort) {
    const DnsHostsKey& key = sort_entry->first;
    const std::vector<IPAddress>& addresses = sort_entry->second;
    address_sorter_->Sort(
        AddressList::CreateFromIPAddressList(addresses,
                                             /*aliases=*/{}),
        base::BindOnce(&WorkItem::OnIndividualAddressSortComplete,
                       weak_ptr_factory_.GetWeakPtr(), key, sort_barrier));
  }
}

void DnsConfigService::HostsReader::WorkItem::OnIndividualAddressSortComplete(
    DnsHostsKey key,
    SortBarrier barrier,
    bool sort_success,
    AddressList sorted) {
  DCHECK(hosts_.has_value());
  DCHECK(hosts_.value().find(key) != hosts_.value().end());
  DCHECK_GE(hosts_.value()[key].size(), 2u);

  if (sort_success) {
    barrier.Run(std::make_pair(std::move(key), std::move(sorted)));
  } else {
    barrier.Run(std::make_pair(std::move(key), AddressList()));
  }
}

void DnsConfigService::HostsReader::WorkItem::OnAddressSortComplete(
    base::OnceClosure closure,
    std::vector<std::pair<DnsHostsKey, AddressList>> sorted) {
  DCHECK(hosts_.has_value());

  for (const std::pair<DnsHostsKey, AddressList>& sorted_host : sorted) {
    auto it = hosts_.value().find(sorted_host.first);
    DCHECK(it != hosts_.value().end());
    DCHECK_GE(it->second.size(), 2u);

    if (sorted_host.second.empty()) {
      // Empty list means sort failure. Remove from hosts.
      hosts_.value().erase(it);
    } else {
      // Replace `hosts_` entry with addresses from `sorted_host`.
      it->second.clear();
      for (const IPEndPoint& endpoint : sorted_host.second) {
        it->second.push_back(endpoint.address());
      }
    }
  }

  std::move(closure).Run();
}

std::unique_ptr<SerialWorker::WorkItem>
DnsConfigService::HostsReader::CreateWorkItem() {
  return std::make_unique<WorkItem>(
      std::make_unique<DnsHostsFileParser>(hosts_file_path_),
      AddressSorter::CreateAddressSorter());
}

void DnsConfigService::HostsReader::OnWorkFinished(
    std::unique_ptr<SerialWorker::WorkItem> serial_worker_work_item) {
  DCHECK(serial_worker_work_item);

  WorkItem* work_item = static_cast<WorkItem*>(serial_worker_work_item.get());
  if (work_item->hosts_.has_value()) {
    service_->OnHostsRead(std::move(work_item->hosts_).value());
  } else {
    LOG(WARNING) << "Failed to read DnsHosts.";
  }
}

void DnsConfigService::ReadHostsNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!hosts_reader_) {
    DCHECK(!hosts_file_path_.empty());
    hosts_reader_ =
        std::make_unique<HostsReader>(hosts_file_path_.value(), *this);
  }
  hosts_reader_->WorkNow();
}

void DnsConfigService::InvalidateConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!have_config_)
    return;
  have_config_ = false;
  StartTimer();
}

void DnsConfigService::InvalidateHosts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!have_hosts_)
    return;
  have_hosts_ = false;
  StartTimer();
}

void DnsConfigService::OnConfigRead(DnsConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValid());

  if (!config.EqualsIgnoreHosts(dns_config_)) {
    dns_config_.CopyIgnoreHosts(config);
    need_update_ = true;
  }

  have_config_ = true;
  if (have_hosts_ || watch_failed_)
    OnCompleteConfig();
}

void DnsConfigService::OnHostsRead(DnsHosts hosts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hosts != dns_config_.hosts) {
    dns_config_.hosts = std::move(hosts);
    need_update_ = true;
  }

  have_hosts_ = true;
  if (have_config_ || watch_failed_)
    OnCompleteConfig();
}

void DnsConfigService::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (last_sent_empty_) {
    DCHECK(!timer_.IsRunning());
    return;  // No need to withdraw again.
  }
  timer_.Stop();

  // Give it a short timeout to come up with a valid config. Otherwise withdraw
  // the config from the receiver. The goal is to avoid perceivable network
  // outage (when using the wrong config) but at the same time avoid
  // unnecessary Job aborts in HostResolverImpl. The signals come from multiple
  // sources so it might receive multiple events during a config change.
  timer_.Start(FROM_HERE, kInvalidationTimeout, this,
               &DnsConfigService::OnTimeout);
}

void DnsConfigService::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!last_sent_empty_);
  // Indicate that even if there is no change in On*Read, we will need to
  // update the receiver when the config becomes complete.
  need_update_ = true;
  // Empty config is considered invalid.
  last_sent_empty_ = true;
  callback_.Run(DnsConfig());
}

void DnsConfigService::OnCompleteConfig() {
  timer_.Stop();
  if (!need_update_)
    return;
  need_update_ = false;
  last_sent_empty_ = false;
  if (watch_failed_) {
    // If a watch failed, the config may not be accurate, so report empty.
    callback_.Run(DnsConfig());
  } else {
    callback_.Run(dns_config_);
  }
}

void DnsConfigService::OnConfigChanged(bool succeeded) {
  if (config_change_delay_) {
    // Ignore transient flutter of config source by delaying the signal a bit.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DnsConfigService::OnConfigChangedDelayed,
                       weak_factory_.GetWeakPtr(), succeeded),
        config_change_delay_.value());
  } else {
    OnConfigChangedDelayed(succeeded);
  }
}

void DnsConfigService::OnHostsChanged(bool succeeded) {
  InvalidateHosts();
  if (succeeded) {
    ReadHostsNow();
  } else {
    LOG(ERROR) << "DNS hosts watch failed.";
    watch_failed_ = true;
  }
}

void DnsConfigService::OnConfigChangedDelayed(bool succeeded) {
  InvalidateConfig();
  if (succeeded) {
    ReadConfigNow();
  } else {
    LOG(ERROR) << "DNS config watch failed.";
    watch_failed_ = true;
  }
}

}  // namespace net
