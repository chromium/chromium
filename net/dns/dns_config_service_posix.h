// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_

#include <netinet/in.h>
#include <resolv.h>
#include <sys/types.h>

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {
struct DnsConfig;

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

// Service for reading and watching POSIX system (except Android and Linux) DNS
// settings.
// This object is not thread-safe and methods may perform blocking I/O so
// methods must be called on a sequence that allows blocking (i.e.
// base::MayBlock). It may be constructed on a different sequence than which
// it's later called on. WatchConfig() must be called prior to ReadConfig().
class NET_EXPORT_PRIVATE DnsConfigServicePosix : public DnsConfigService {
 public:
  DnsConfigServicePosix();

  DnsConfigServicePosix(const DnsConfigServicePosix&) = delete;
  DnsConfigServicePosix& operator=(const DnsConfigServicePosix&) = delete;

  ~DnsConfigServicePosix() override;

  void RefreshConfig() override;

 protected:
  // DnsConfigService:
  void ReadConfigNow() override;
  bool StartWatching() override;

  // Create |config_reader_|.
  void CreateReader();

 private:
  FRIEND_TEST_ALL_PREFIXES(DnsConfigServicePosixTest,
                           ChangeConfigMultipleTimes);
  class Watcher;
  class ConfigReader;

  std::unique_ptr<Watcher> watcher_;
  std::unique_ptr<ConfigReader> config_reader_;
};

// Returns nullopt iff a valid config could not be determined.
std::optional<DnsConfig> NET_EXPORT_PRIVATE
ConvertResStateToDnsConfig(const struct __res_state& res);

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
