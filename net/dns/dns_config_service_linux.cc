// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_linux.h"

#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config.h"
#include "net/dns/nsswitch_reader.h"
#include "net/dns/serial_worker.h"

namespace net {

namespace internal {

namespace {

const base::FilePath::CharType kFilePathHosts[] =
    FILE_PATH_LITERAL("/etc/hosts");

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF FILE_PATH_LITERAL("/etc/resolv.conf")
#endif

constexpr base::FilePath::CharType kFilePathResolv[] = _PATH_RESCONF;

#ifndef _PATH_NSSWITCH_CONF  // Normally defined in <netdb.h>
#define _PATH_NSSWITCH_CONF FILE_PATH_LITERAL("/etc/nsswitch.conf")
#endif

constexpr base::FilePath::CharType kFilePathNsswitch[] = _PATH_NSSWITCH_CONF;

base::Optional<DnsConfig> ConvertResStateToDnsConfig(
    const struct __res_state& res) {
  DnsConfig dns_config;
  dns_config.unhandled_options = false;

  if (!(res.options & RES_INIT))
    return base::nullopt;

  static_assert(std::extent<decltype(res.nsaddr_list)>() >= MAXNS &&
                    std::extent<decltype(res._u._ext.nsaddrs)>() >= MAXNS,
                "incompatible libresolv res_state");
  DCHECK_LE(res.nscount, MAXNS);
  // Initially, glibc stores IPv6 in |_ext.nsaddrs| and IPv4 in |nsaddr_list|.
  // In res_send.c:res_nsend, it merges |nsaddr_list| into |nsaddrs|,
  // but we have to combine the two arrays ourselves.
  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    const struct sockaddr* addr = nullptr;
    size_t addr_len = 0;
    if (res.nsaddr_list[i].sin_family) {  // The indicator used by res_nsend.
      addr = reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]);
      addr_len = sizeof res.nsaddr_list[i];
    } else if (res._u._ext.nsaddrs[i]) {
      addr = reinterpret_cast<const struct sockaddr*>(res._u._ext.nsaddrs[i]);
      addr_len = sizeof *res._u._ext.nsaddrs[i];
    } else {
      return base::nullopt;
    }
    if (!ipe.FromSockAddr(addr, addr_len))
      return base::nullopt;
    dns_config.nameservers.push_back(ipe);
  }

  dns_config.search.clear();
  for (int i = 0; (i < MAXDNSRCH) && res.dnsrch[i]; ++i) {
    dns_config.search.emplace_back(res.dnsrch[i]);
  }

  dns_config.ndots = res.ndots;
  dns_config.fallback_period = base::TimeDelta::FromSeconds(res.retrans);
  dns_config.attempts = res.retry;
#if defined(RES_ROTATE)
  dns_config.rotate = res.options & RES_ROTATE;
#endif
#if !defined(RES_USE_DNSSEC)
  // Some versions of libresolv don't have support for the DO bit. In this
  // case, we proceed without it.
  static const int RES_USE_DNSSEC = 0;
#endif

  // The current implementation assumes these options are set. They normally
  // cannot be overwritten by /etc/resolv.conf
  const unsigned kRequiredOptions = RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  if ((res.options & kRequiredOptions) != kRequiredOptions) {
    dns_config.unhandled_options = true;
    return dns_config;
  }

  const unsigned kUnhandledOptions = RES_USEVC | RES_IGNTC | RES_USE_DNSSEC;
  if (res.options & kUnhandledOptions) {
    dns_config.unhandled_options = true;
    return dns_config;
  }

  if (dns_config.nameservers.empty())
    return base::nullopt;

  // If any name server is 0.0.0.0, assume the configuration is invalid.
  for (const IPEndPoint& nameserver : dns_config.nameservers) {
    if (nameserver.address().IsZero())
      return base::nullopt;
  }
  return dns_config;
}

bool IsNsswitchConfigCompatible(
    const std::vector<NsswitchReader::ServiceSpecification>& nsswitch_hosts) {
  // TODO(crbug.com/117655): Implement.
  return true;
}

}  // namespace

class DnsConfigServiceLinux::Watcher : public DnsConfigService::Watcher {
 public:
  explicit Watcher(DnsConfigServiceLinux& service)
      : DnsConfigService::Watcher(service) {}
  ~Watcher() override = default;

  Watcher(const Watcher&) = delete;
  Watcher& operator=(const Watcher&) = delete;

  bool Watch() override {
    CheckOnCorrectSequence();

    bool success = true;
    if (!resolv_watcher_.Watch(
            base::FilePath(kFilePathResolv),
            base::FilePathWatcher::Type::kNonRecursive,
            base::BindRepeating(&Watcher::OnConfigFilePathWatcherChange,
                                base::Unretained(this)))) {
      LOG(ERROR) << "DNS config (resolv.conf) watch failed to start.";
      success = false;
    }

    if (!nsswitch_watcher_.Watch(
            base::FilePath(kFilePathNsswitch),
            base::FilePathWatcher::Type::kNonRecursive,
            base::BindRepeating(&Watcher::OnConfigFilePathWatcherChange,
                                base::Unretained(this)))) {
      LOG(ERROR) << "DNS nsswitch.conf watch failed to start.";
      success = false;
    }

    if (!hosts_watcher_.Watch(
            base::FilePath(kFilePathHosts),
            base::FilePathWatcher::Type::kNonRecursive,
            base::BindRepeating(&Watcher::OnHostsFilePathWatcherChange,
                                base::Unretained(this)))) {
      LOG(ERROR) << "DNS hosts watch failed to start.";
      success = false;
    }
    return success;
  }

 private:
  void OnConfigFilePathWatcherChange(const base::FilePath& path, bool error) {
    OnConfigChanged(!error);
  }

  void OnHostsFilePathWatcherChange(const base::FilePath& path, bool error) {
    OnHostsChanged(!error);
  }

  base::FilePathWatcher resolv_watcher_;
  base::FilePathWatcher nsswitch_watcher_;
  base::FilePathWatcher hosts_watcher_;
};

// A SerialWorker that uses libresolv to initialize res_state and converts
// it to DnsConfig.
class DnsConfigServiceLinux::ConfigReader : public SerialWorker {
 public:
  explicit ConfigReader(DnsConfigServiceLinux& service,
                        std::unique_ptr<ResolvReader> resolv_reader,
                        std::unique_ptr<NsswitchReader> nsswitch_reader)
      : service_(&service),
        resolv_reader_(std::move(resolv_reader)),
        nsswitch_reader_(std::move(nsswitch_reader)) {
    // Allow execution on another thread; nothing thread-specific about
    // constructor.
    DETACH_FROM_SEQUENCE(sequence_checker_);

    DCHECK(resolv_reader_);
    DCHECK(nsswitch_reader_);
  }

  ConfigReader(const ConfigReader&) = delete;
  ConfigReader& operator=(const ConfigReader&) = delete;

  void DoWork() override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    std::unique_ptr<struct __res_state> res = resolv_reader_->GetResState();
    if (res) {
      dns_config_ = ConvertResStateToDnsConfig(*res.get());
      resolv_reader_->CloseResState(res.get());
    }

    if (!dns_config_.has_value())
      return;

    // Override `fallback_period` value to match default setting on Windows.
    dns_config_->fallback_period = kDnsDefaultFallbackPeriod;

    if (dns_config_ && !dns_config_->unhandled_options) {
      std::vector<NsswitchReader::ServiceSpecification> nsswitch_hosts =
          nsswitch_reader_->ReadAndParseHosts();
      dns_config_->unhandled_options =
          !IsNsswitchConfigCompatible(nsswitch_hosts);
    }
  }

  void OnWorkFinished() override {
    DCHECK(!IsCancelled());
    if (dns_config_.has_value()) {
      service_->OnConfigRead(std::move(dns_config_).value());
    } else {
      LOG(WARNING) << "Failed to read DnsConfig.";
    }
  }

 private:
  ~ConfigReader() override = default;

  // Raw pointer to owning DnsConfigService. This must never be accessed inside
  // DoWork(), since service may be destroyed while SerialWorker is running
  // on worker thread.
  DnsConfigServiceLinux* const service_;
  // Written/accessed in DoWork, read in OnWorkFinished, no locking necessary.
  base::Optional<DnsConfig> dns_config_;
  std::unique_ptr<ResolvReader> resolv_reader_;
  std::unique_ptr<NsswitchReader> nsswitch_reader_;
};

std::unique_ptr<struct __res_state>
DnsConfigServiceLinux::ResolvReader::GetResState() {
  auto res = std::make_unique<struct __res_state>();
  memset(res.get(), 0, sizeof(struct __res_state));

  if (res_ninit(res.get()) != 0) {
    CloseResState(res.get());
    return nullptr;
  }

  return res;
}

void DnsConfigServiceLinux::ResolvReader::CloseResState(
    struct __res_state* res) {
  res_nclose(res);
}

DnsConfigServiceLinux::DnsConfigServiceLinux()
    : DnsConfigService(kFilePathHosts) {
  // Allow constructing on one thread and living on another.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DnsConfigServiceLinux::~DnsConfigServiceLinux() {
  if (config_reader_)
    config_reader_->Cancel();
}

void DnsConfigServiceLinux::ReadConfigNow() {
  if (!config_reader_)
    CreateReader();
  config_reader_->WorkNow();
}

bool DnsConfigServiceLinux::StartWatching() {
  CreateReader();
  watcher_ = std::make_unique<Watcher>(*this);
  return watcher_->Watch();
}

void DnsConfigServiceLinux::CreateReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!config_reader_);
  DCHECK(resolv_reader_);
  DCHECK(nsswitch_reader_);
  config_reader_ = base::MakeRefCounted<ConfigReader>(
      *this, std::move(resolv_reader_), std::move(nsswitch_reader_));
}

}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return std::make_unique<internal::DnsConfigServiceLinux>();
}

}  // namespace net
