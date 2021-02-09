// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <resolv.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_service_posix.h"
#include "net/dns/public/dns_protocol.h"

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#endif  // defined(OS_ANDROID)

// Required for inet_pton()
#if defined(OS_WIN)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace net {

namespace {

// MAXNS is normally 3, but let's test 4 if possible.
const char* const kNameserversIPv4[] = {
    "8.8.8.8",
    "192.168.1.1",
    "63.1.2.4",
    "1.0.0.1",
};

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
const char* const kNameserversIPv6[] = {
    NULL,
    "2001:DB8:0::42",
    NULL,
    "::FFFF:129.144.52.38",
};
#endif

void DummyConfigCallback(const DnsConfig& config) {
  // Do nothing
}

// Fills in |res| with sane configuration.
void InitializeResState(res_state res) {
  memset(res, 0, sizeof(*res));
  res->options = RES_INIT | RES_RECURSE | RES_DEFNAMES | RES_DNSRCH |
                 RES_ROTATE;
  res->ndots = 2;
  res->retrans = 4;
  res->retry = 7;

  const char kDnsrch[] = "chromium.org" "\0" "example.com";
  memcpy(res->defdname, kDnsrch, sizeof(kDnsrch));
  res->dnsrch[0] = res->defdname;
  res->dnsrch[1] = res->defdname + sizeof("chromium.org");

  for (unsigned i = 0; i < base::size(kNameserversIPv4) && i < MAXNS; ++i) {
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = base::HostToNet16(NS_DEFAULTPORT + i);
    inet_pton(AF_INET, kNameserversIPv4[i], &sa.sin_addr);
    res->nsaddr_list[i] = sa;
    ++res->nscount;
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Install IPv6 addresses, replacing the corresponding IPv4 addresses.
  unsigned nscount6 = 0;
  for (unsigned i = 0; i < base::size(kNameserversIPv6) && i < MAXNS; ++i) {
    if (!kNameserversIPv6[i])
      continue;
    // Must use malloc to mimick res_ninit.
    struct sockaddr_in6 *sa6;
    sa6 = (struct sockaddr_in6 *)malloc(sizeof(*sa6));
    sa6->sin6_family = AF_INET6;
    sa6->sin6_port = base::HostToNet16(NS_DEFAULTPORT - i);
    inet_pton(AF_INET6, kNameserversIPv6[i], &sa6->sin6_addr);
    res->_u._ext.nsaddrs[i] = sa6;
    memset(&res->nsaddr_list[i], 0, sizeof res->nsaddr_list[i]);
    ++nscount6;
  }
  res->_u._ext.nscount6 = nscount6;
#endif
}

void CloseResState(res_state res) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  for (int i = 0; i < res->nscount; ++i) {
    if (res->_u._ext.nsaddrs[i] != NULL)
      free(res->_u._ext.nsaddrs[i]);
  }
#endif
}

void InitializeExpectedConfig(DnsConfig* config) {
  config->ndots = 2;
  config->fallback_period = base::TimeDelta::FromSeconds(4);
  config->attempts = 7;
  config->rotate = true;
  config->append_to_multi_label_name = true;
  config->search.clear();
  config->search.push_back("chromium.org");
  config->search.push_back("example.com");

  config->nameservers.clear();
  for (unsigned i = 0; i < base::size(kNameserversIPv4) && i < MAXNS; ++i) {
    IPAddress ip;
    EXPECT_TRUE(ip.AssignFromIPLiteral(kNameserversIPv4[i]));
    config->nameservers.push_back(IPEndPoint(ip, NS_DEFAULTPORT + i));
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  for (unsigned i = 0; i < base::size(kNameserversIPv6) && i < MAXNS; ++i) {
    if (!kNameserversIPv6[i])
      continue;
    IPAddress ip;
    EXPECT_TRUE(ip.AssignFromIPLiteral(kNameserversIPv6[i]));
    config->nameservers[i] = IPEndPoint(ip, NS_DEFAULTPORT - i);
  }
#endif
}

TEST(DnsConfigServicePosixTest, ConvertResStateToDnsConfig) {
  struct __res_state res;
  InitializeResState(&res);
  base::Optional<DnsConfig> config = internal::ConvertResStateToDnsConfig(res);
  CloseResState(&res);
  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());

  DnsConfig expected_config;
  EXPECT_FALSE(expected_config.EqualsIgnoreHosts(config.value()));
  InitializeExpectedConfig(&expected_config);
  EXPECT_TRUE(expected_config.EqualsIgnoreHosts(config.value()));
}

TEST(DnsConfigServicePosixTest, RejectEmptyNameserver) {
  struct __res_state res = {};
  res.options = RES_INIT | RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  const char kDnsrch[] = "chromium.org";
  memcpy(res.defdname, kDnsrch, sizeof(kDnsrch));
  res.dnsrch[0] = res.defdname;

  struct sockaddr_in sa = {};
  sa.sin_family = AF_INET;
  sa.sin_port = base::HostToNet16(NS_DEFAULTPORT);
  sa.sin_addr.s_addr = INADDR_ANY;
  res.nsaddr_list[0] = sa;
  sa.sin_addr.s_addr = 0xCAFE1337;
  res.nsaddr_list[1] = sa;
  res.nscount = 2;

  EXPECT_FALSE(internal::ConvertResStateToDnsConfig(res));

  sa.sin_addr.s_addr = 0xDEADBEEF;
  res.nsaddr_list[0] = sa;
  EXPECT_TRUE(internal::ConvertResStateToDnsConfig(res));
}

TEST(DnsConfigServicePosixTest, DestroyWhileJobsWorking) {
  // Regression test to verify crash does not occur if DnsConfigServicePosix
  // instance is destroyed while SerialWorker jobs have posted to worker pool.
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<internal::DnsConfigServicePosix> service(
      new internal::DnsConfigServicePosix());
  // Call WatchConfig() which also tests ReadConfig().
  service->WatchConfig(base::BindRepeating(&DummyConfigCallback));
  service.reset();
  task_environment.RunUntilIdle();
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1000));
}

TEST(DnsConfigServicePosixTest, DestroyOnDifferentThread) {
  // Regression test to verify crash does not occur if DnsConfigServicePosix
  // instance is destroyed on another thread.
  base::test::TaskEnvironment task_environment;

  scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  std::unique_ptr<internal::DnsConfigServicePosix, base::OnTaskRunnerDeleter>
      service(new internal::DnsConfigServicePosix(),
              base::OnTaskRunnerDeleter(runner));

  runner->PostTask(FROM_HERE,
                   base::BindOnce(&internal::DnsConfigServicePosix::WatchConfig,
                                  base::Unretained(service.get()),
                                  base::BindRepeating(&DummyConfigCallback)));
  service.reset();
  task_environment.RunUntilIdle();
}

}  // namespace


}  // namespace net
