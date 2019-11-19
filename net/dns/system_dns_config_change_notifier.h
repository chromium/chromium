// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_SYSTEM_DNS_CONFIG_CHANGE_NOTIFIER_H_
#define NET_DNS_SYSTEM_DNS_CONFIG_CHANGE_NOTIFIER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config.h"

namespace net {

class DnsConfigService;

// Notifier that can be subscribed to to listen for changes to system DNS
// configuration. Expected to only be used internally to HostResolverManager and
// NetworkChangeNotifier. Other classes are expected to subscribe to
// NetworkChangeNotifier::AddDNSObserver() to subscribe to listen to both system
// config changes and configuration applied on top by Chrome.
//
// This class is thread and sequence safe except that RemoveObserver() must be
// called on the same sequence as the matched AddObserver() call.
//
// TODO(crbug.com/971411): Use this class in HostResolverManager.
class NET_EXPORT_PRIVATE SystemDnsConfigChangeNotifier {
 public:
  class Observer {
   public:
    // Called on loading new config, including the initial read once the first
    // valid config has been read. If a config read encounters errors or an
    // invalid config is read, will be invoked with |base::nullopt|. Only
    // invoked when |config| changes.
    virtual void OnSystemDnsConfigChanged(base::Optional<DnsConfig> config) = 0;
  };

  SystemDnsConfigChangeNotifier();
  // Alternate constructor allowing specifying the underlying DnsConfigService.
  // |dns_config_service| will only be interacted with and destroyed using
  // |task_runner|. As required by DnsConfigService, blocking I/O may be
  // performed on |task_runner|, so it must support blocking (i.e.
  // base::MayBlock).
  //
  // |dns_config_service| may be null if system DNS config is disabled for the
  // current platform. Calls against the created object will noop, and no
  // notifications will ever be sent.
  SystemDnsConfigChangeNotifier(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<DnsConfigService> dns_config_service);
  ~SystemDnsConfigChangeNotifier();

  // An added Observer will receive notifications on the sequence where
  // AddObserver() was called. If the config has been successfully read before
  // calling this method, a notification will be sent for that current config
  // before any other notifications.
  void AddObserver(Observer* observer);

  // In order to ensure notifications immediately stop on calling
  // RemoveObserver(), must be called on the same sequence where the associated
  // AddObserver() was called.
  void RemoveObserver(Observer* observer);

  // Triggers invalidation and re-read of the current configuration (followed by
  // notifications to registered Observers). For use only on platforms
  // expecting network-stack-external notifications of DNS config changes.
  void RefreshConfig();

  void SetDnsConfigServiceForTesting(
      std::unique_ptr<DnsConfigService> dns_config_service);

 private:
  class Core;

  std::unique_ptr<Core, base::OnTaskRunnerDeleter> core_;

  DISALLOW_COPY_AND_ASSIGN(SystemDnsConfigChangeNotifier);
};

}  // namespace net

#endif  // NET_DNS_SYSTEM_DNS_CONFIG_CHANGE_NOTIFIER_H_
