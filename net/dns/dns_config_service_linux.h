// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_LINUX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_LINUX_H_

#include <arpa/inet.h>
#include <resolv.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/nsswitch_reader.h"

namespace net {

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

// Service for reading and watching Linux  DNS settings.
// This object is not thread-safe and methods may perform blocking I/O so
// methods must be called on a sequence that allows blocking (i.e.
// base::MayBlock). It may be constructed on a different sequence than which
// it's later called on. WatchConfig() must be called prior to ReadConfig().
class NET_EXPORT_PRIVATE DnsConfigServiceLinux : public DnsConfigService {
 public:
  // Test-overridable class to handle the interactions with OS APIs for reading
  // resolv.conf.
  class NET_EXPORT_PRIVATE ResolvReader {
   public:
    virtual ~ResolvReader() = default;

    // Null` on failure. If not null, result must be cleaned up through a call
    // to `CloseResState()`.
    virtual std::unique_ptr<struct __res_state> GetResState();
    virtual void CloseResState(struct __res_state* res);
  };

  DnsConfigServiceLinux();
  ~DnsConfigServiceLinux() override;

  DnsConfigServiceLinux(const DnsConfigServiceLinux&) = delete;
  DnsConfigServiceLinux& operator=(const DnsConfigServiceLinux&) = delete;

  void set_resolv_reader_for_testing(
      std::unique_ptr<ResolvReader> resolv_reader) {
    DCHECK(!config_reader_);  // Need to call before first read.
    DCHECK(resolv_reader);
    resolv_reader_ = std::move(resolv_reader);
  }

  void set_nsswitch_reader_for_testing(
      std::unique_ptr<NsswitchReader> nsswitch_reader) {
    DCHECK(!config_reader_);  // Need to call before first read.
    DCHECK(nsswitch_reader);
    nsswitch_reader_ = std::move(nsswitch_reader);
  }

 protected:
  // DnsConfigService:
  void ReadConfigNow() override;
  bool StartWatching() override;

  // Create |config_reader_|.
  void CreateReader();

 private:
  FRIEND_TEST_ALL_PREFIXES(DnsConfigServiceLinuxTest,
                           ChangeConfigMultipleTimes);
  class Watcher;
  class ConfigReader;

  std::unique_ptr<ResolvReader> resolv_reader_ =
      std::make_unique<ResolvReader>();
  std::unique_ptr<NsswitchReader> nsswitch_reader_ =
      std::make_unique<NsswitchReader>();

  std::unique_ptr<Watcher> watcher_;
  scoped_refptr<ConfigReader> config_reader_;
};

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_LINUX_H_
