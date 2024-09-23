// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/serial_worker.h"

namespace net {

// static
const base::TimeDelta DnsConfigService::kInvalidationTimeout =
    base::Milliseconds(150);

DnsConfigService::DnsConfigService(
    base::FilePath::StringPieceType hosts_file_path,
    std::optional<base::TimeDelta> config_change_delay)
    : config_change_delay_(config_change_delay),
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
  NOTREACHED_IN_MIGRATION();
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
    std::unique_ptr<DnsHostsParser> dns_hosts_parser)
    : dns_hosts_parser_(std::move(dns_hosts_parser)) {
  DCHECK(dns_hosts_parser_);
}

DnsConfigService::HostsReader::WorkItem::~WorkItem() = default;

std::optional<DnsHosts> DnsConfigService::HostsReader::WorkItem::ReadHosts() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DnsHosts dns_hosts;
  if (!dns_hosts_parser_->ParseHosts(&dns_hosts))
    return std::nullopt;

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

std::unique_ptr<SerialWorker::WorkItem>
DnsConfigService::HostsReader::CreateWorkItem() {
  return std::make_unique<WorkItem>(
      std::make_unique<DnsHostsFileParser>(hosts_file_path_));
}

bool DnsConfigService::HostsReader::OnWorkFinished(
    std::unique_ptr<SerialWorker::WorkItem> serial_worker_work_item) {
  DCHECK(serial_worker_work_item);

  WorkItem* work_item = static_cast<WorkItem*>(serial_worker_work_item.get());
  if (work_item->hosts_.has_value()) {
    service_->OnHostsRead(std::move(work_item->hosts_).value());
    return true;
  } else {
    LOG(WARNING) << "Failed to read DnsHosts.";
    return false;
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
  timer_.AbandonAndStop();
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
