// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_

#include <memory>

#if !defined(OS_ANDROID)
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#endif

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {
struct DnsConfig;

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

// Service for reading and watching POSIX system (except Android) DNS settings.
// This object is not thread-safe and methods may perform blocking I/O so
// methods must be called on a sequence that allows blocking (i.e.
// base::MayBlock). It may be constructed on a different sequence than which
// it's later called on. WatchConfig() must be called prior to ReadConfig().
class NET_EXPORT_PRIVATE DnsConfigServicePosix : public DnsConfigService {
 public:
  DnsConfigServicePosix();
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
  scoped_refptr<ConfigReader> config_reader_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServicePosix);
};

#if !defined(OS_ANDROID)
// Returns nullopt iff a valid config could not be determined.
base::Optional<DnsConfig> NET_EXPORT_PRIVATE
ConvertResStateToDnsConfig(const struct __res_state& res);
#endif

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
