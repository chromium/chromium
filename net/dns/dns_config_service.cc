// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace net {

DnsConfigService::DnsConfigService()
    : watch_failed_(false),
      have_config_(false),
      have_hosts_(false),
      need_update_(false),
      last_sent_empty_(true) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DnsConfigService::~DnsConfigService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DnsConfigService::ReadConfig(const CallbackType& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());
  callback_ = callback;
  ReadNow();
}

void DnsConfigService::WatchConfig(const CallbackType& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());
  callback_ = callback;
  watch_failed_ = !StartWatching();
  ReadNow();
}

void DnsConfigService::RefreshConfig() {
  // Overridden on supported platforms.
  NOTREACHED();
}

void DnsConfigService::InvalidateConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_invalidate_config_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("AsyncDNS.ConfigNotifyInterval",
                             now - last_invalidate_config_time_);
  }
  last_invalidate_config_time_ = now;
  if (!have_config_)
    return;
  have_config_ = false;
  StartTimer();
}

void DnsConfigService::InvalidateHosts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_invalidate_hosts_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("AsyncDNS.HostsNotifyInterval",
                             now - last_invalidate_hosts_time_);
  }
  last_invalidate_hosts_time_ = now;
  if (!have_hosts_)
    return;
  have_hosts_ = false;
  StartTimer();
}

void DnsConfigService::OnConfigRead(const DnsConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValid());

  bool changed = false;
  if (!config.EqualsIgnoreHosts(dns_config_)) {
    dns_config_.CopyIgnoreHosts(config);
    need_update_ = true;
    changed = true;
  }
  if (!changed && !last_sent_empty_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("AsyncDNS.UnchangedConfigInterval",
                             base::TimeTicks::Now() - last_sent_empty_time_);
  }
  UMA_HISTOGRAM_BOOLEAN("AsyncDNS.ConfigChange", changed);

  have_config_ = true;
  if (have_hosts_ || watch_failed_)
    OnCompleteConfig();
}

void DnsConfigService::OnHostsRead(const DnsHosts& hosts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool changed = false;
  if (hosts != dns_config_.hosts) {
    dns_config_.hosts = hosts;
    need_update_ = true;
    changed = true;
  }
  if (!changed && !last_sent_empty_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("AsyncDNS.UnchangedHostsInterval",
                             base::TimeTicks::Now() - last_sent_empty_time_);
  }
  UMA_HISTOGRAM_BOOLEAN("AsyncDNS.HostsChange", changed);

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

  // DHCP and user-induced changes are on the order of seconds, so 150ms should
  // not add perceivable delay. On the other hand, config readers should finish
  // within 150ms with the rare exception of I/O block or extra large HOSTS.
  const base::TimeDelta kTimeout = base::TimeDelta::FromMilliseconds(150);

  timer_.Start(FROM_HERE,
               kTimeout,
               this,
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
  last_sent_empty_time_ = base::TimeTicks::Now();
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

}  // namespace net
