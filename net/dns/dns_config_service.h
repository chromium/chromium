// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_H_
#define NET_DNS_DNS_CONFIG_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"
#include "url/gurl.h"

namespace net {

// Service for reading system DNS settings, on demand or when signalled by
// internal watchers and NetworkChangeNotifier. This object is not thread-safe
// and methods may perform blocking I/O so methods must be called on a sequence
// that allows blocking (i.e. base::MayBlock).
class NET_EXPORT_PRIVATE DnsConfigService {
 public:
  // Callback interface for the client, called on the same thread as
  // ReadConfig() and WatchConfig().
  typedef base::Callback<void(const DnsConfig& config)> CallbackType;

  // Creates the platform-specific DnsConfigService. May return |nullptr| if
  // reading system DNS settings is not supported on the current platform.
  static std::unique_ptr<DnsConfigService> CreateSystemService();

  DnsConfigService();
  virtual ~DnsConfigService();

  // Attempts to read the configuration. Will run |callback| when succeeded.
  // Can be called at most once.
  void ReadConfig(const CallbackType& callback);

  // Registers systems watchers. Will attempt to read config after watch starts,
  // but only if watchers started successfully. Will run |callback| iff config
  // changes from last call or has to be withdrawn. Can be called at most once.
  // Might require MessageLoopForIO.
  void WatchConfig(const CallbackType& callback);

  // Triggers invalidation and re-read of the current configuration (followed by
  // invocation of the callback). For use only on platforms expecting
  // network-stack-external notifications of DNS config changes.
  virtual void RefreshConfig();

 protected:
  enum WatchStatus {
    DNS_CONFIG_WATCH_STARTED = 0,
    DNS_CONFIG_WATCH_FAILED_TO_START_CONFIG,
    DNS_CONFIG_WATCH_FAILED_TO_START_HOSTS,
    DNS_CONFIG_WATCH_FAILED_CONFIG,
    DNS_CONFIG_WATCH_FAILED_HOSTS,
    DNS_CONFIG_WATCH_MAX,
  };

  // Immediately attempts to read the current configuration.
  virtual void ReadNow() = 0;
  // Registers system watchers. Returns true iff succeeds.
  virtual bool StartWatching() = 0;

  // Called when the current config (except hosts) has changed.
  void InvalidateConfig();
  // Called when the current hosts have changed.
  void InvalidateHosts();

  // Called with new config. |config|.hosts is ignored.
  void OnConfigRead(const DnsConfig& config);
  // Called with new hosts. Rest of the config is assumed unchanged.
  void OnHostsRead(const DnsHosts& hosts);

  void set_watch_failed(bool value) { watch_failed_ = value; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // The timer counts from the last Invalidate* until complete config is read.
  void StartTimer();
  void OnTimeout();
  // Called when the config becomes complete. Stops the timer.
  void OnCompleteConfig();

  CallbackType callback_;

  DnsConfig dns_config_;

  // True if any of the necessary watchers failed. In that case, the service
  // will communicate changes via OnTimeout, but will only send empty DnsConfig.
  bool watch_failed_;
  // True after On*Read, before Invalidate*. Tells if the config is complete.
  bool have_config_;
  bool have_hosts_;
  // True if receiver needs to be updated when the config becomes complete.
  bool need_update_;
  // True if the last config sent was empty (instead of |dns_config_|).
  // Set when |timer_| expires.
  bool last_sent_empty_;

  // Initialized and updated on Invalidate* call.
  base::TimeTicks last_invalidate_config_time_;
  base::TimeTicks last_invalidate_hosts_time_;
  // Initialized and updated when |timer_| expires.
  base::TimeTicks last_sent_empty_time_;

  // Started in Invalidate*, cleared in On*Read.
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigService);
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_H_
