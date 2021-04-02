// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_LINUX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_LINUX_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

struct __res_state;

namespace net {
struct DnsConfig;

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

// Service for reading and watching Linux  DNS settings.
// This object is not thread-safe and methods may perform blocking I/O so
// methods must be called on a sequence that allows blocking (i.e.
// base::MayBlock). It may be constructed on a different sequence than which
// it's later called on. WatchConfig() must be called prior to ReadConfig().
class NET_EXPORT_PRIVATE DnsConfigServiceLinux : public DnsConfigService {
 public:
  DnsConfigServiceLinux();
  ~DnsConfigServiceLinux() override;

  DnsConfigServiceLinux(const DnsConfigServiceLinux&) = delete;
  DnsConfigServiceLinux& operator=(const DnsConfigServiceLinux&) = delete;

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

  std::unique_ptr<Watcher> watcher_;
  scoped_refptr<ConfigReader> config_reader_;
};

// Returns nullopt iff a valid config could not be determined.
base::Optional<DnsConfig> NET_EXPORT_PRIVATE
ConvertResStateToDnsConfig(const struct __res_state& res);

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_LINUX_H_
