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
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {
struct DnsConfig;

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

// Service for reading and watching POSIX system DNS settings. This object is
// not thread-safe and methods may perform blocking I/O so methods must be
// called on a sequence that allows blocking (i.e. base::MayBlock). It may be
// constructed on a different sequence than which it's later called on.
// WatchConfig() must be called prior to ReadConfig().
class NET_EXPORT_PRIVATE DnsConfigServicePosix : public DnsConfigService {
 public:
  DnsConfigServicePosix();
  ~DnsConfigServicePosix() override;

  void RefreshConfig() override;

 protected:
  // DnsConfigService:
  void ReadNow() override;
  bool StartWatching() override;

  // Create |config_reader_| and |hosts_reader_|.
  void CreateReaders();

 private:
  FRIEND_TEST_ALL_PREFIXES(DnsConfigServicePosixTest,
                           ChangeConfigMultipleTimes);
  class Watcher;
  class ConfigReader;
  class HostsReader;

  void OnConfigChanged(bool succeeded);
  void OnHostsChanged(bool succeeded);

  std::unique_ptr<Watcher> watcher_;
  // Allow a mock hosts file for testing purposes.
  const base::FilePath::CharType* file_path_hosts_;
  scoped_refptr<ConfigReader> config_reader_;
  scoped_refptr<HostsReader> hosts_reader_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServicePosix);
};

enum ConfigParsePosixResult {
  CONFIG_PARSE_POSIX_OK = 0,
  CONFIG_PARSE_POSIX_RES_INIT_FAILED,
  CONFIG_PARSE_POSIX_RES_INIT_UNSET,
  CONFIG_PARSE_POSIX_BAD_ADDRESS,
  CONFIG_PARSE_POSIX_BAD_EXT_STRUCT,
  CONFIG_PARSE_POSIX_NULL_ADDRESS,
  CONFIG_PARSE_POSIX_NO_NAMESERVERS,
  CONFIG_PARSE_POSIX_MISSING_OPTIONS,
  CONFIG_PARSE_POSIX_UNHANDLED_OPTIONS,
  CONFIG_PARSE_POSIX_NO_DNSINFO,
  CONFIG_PARSE_POSIX_MAX  // Bounding values for enumeration.
};

#if !defined(OS_ANDROID)
// Fills in |dns_config| from |res|.
ConfigParsePosixResult NET_EXPORT_PRIVATE ConvertResStateToDnsConfig(
    const struct __res_state& res, DnsConfig* dns_config);
#endif

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
