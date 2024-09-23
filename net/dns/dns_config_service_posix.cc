// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_config_service_posix.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/notify_watcher_mac.h"
#include "net/dns/public/resolv_reader.h"
#include "net/dns/serial_worker.h"

#if BUILDFLAG(IS_MAC)
#include "net/dns/dns_config_watcher_mac.h"
#endif

namespace net {

namespace internal {

namespace {

const base::FilePath::CharType kFilePathHosts[] =
    FILE_PATH_LITERAL("/etc/hosts");

#if BUILDFLAG(IS_IOS)
// There is no public API to watch the DNS configuration on iOS.
class DnsConfigWatcher {
 public:
  using CallbackType = base::RepeatingCallback<void(bool succeeded)>;

  bool Watch(const CallbackType& callback) {
    return false;
  }
};

#elif BUILDFLAG(IS_MAC)

// DnsConfigWatcher for OS_MAC is in dns_config_watcher_mac.{hh,cc}.

#else  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_MAC)

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF "/etc/resolv.conf"
#endif

const base::FilePath::CharType kFilePathConfig[] =
    FILE_PATH_LITERAL(_PATH_RESCONF);

class DnsConfigWatcher {
 public:
  using CallbackType = base::RepeatingCallback<void(bool succeeded)>;

  bool Watch(const CallbackType& callback) {
    callback_ = callback;
    return watcher_.Watch(base::FilePath(kFilePathConfig),
                          base::FilePathWatcher::Type::kNonRecursive,
                          base::BindRepeating(&DnsConfigWatcher::OnCallback,
                                              base::Unretained(this)));
  }

 private:
  void OnCallback(const base::FilePath& path, bool error) {
    callback_.Run(!error);
  }

  base::FilePathWatcher watcher_;
  CallbackType callback_;
};
#endif  // BUILDFLAG(IS_IOS)

std::optional<DnsConfig> ReadDnsConfig() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::optional<DnsConfig> dns_config;
  {
    std::unique_ptr<ScopedResState> scoped_res_state =
        ResolvReader().GetResState();
    if (scoped_res_state) {
      dns_config = ConvertResStateToDnsConfig(scoped_res_state->state());
    }
  }

  if (!dns_config.has_value())
    return dns_config;

#if BUILDFLAG(IS_MAC)
  if (!DnsConfigWatcher::CheckDnsConfig(
          dns_config->unhandled_options /* out_unhandled_options */)) {
    return std::nullopt;
  }
#endif  // BUILDFLAG(IS_MAC)
  // Override |fallback_period| value to match default setting on Windows.
  dns_config->fallback_period = kDnsDefaultFallbackPeriod;
  return dns_config;
}

}  // namespace

class DnsConfigServicePosix::Watcher : public DnsConfigService::Watcher {
 public:
  explicit Watcher(DnsConfigServicePosix& service)
      : DnsConfigService::Watcher(service) {}

  Watcher(const Watcher&) = delete;
  Watcher& operator=(const Watcher&) = delete;

  ~Watcher() override = default;

  bool Watch() override {
    CheckOnCorrectSequence();

    bool success = true;
    if (!config_watcher_.Watch(base::BindRepeating(&Watcher::OnConfigChanged,
                                                   base::Unretained(this)))) {
      LOG(ERROR) << "DNS config watch failed to start.";
      success = false;
    }
// Hosts file should never change on iOS, so don't watch it there.
#if !BUILDFLAG(IS_IOS)
    if (!hosts_watcher_.Watch(
            base::FilePath(kFilePathHosts),
            base::FilePathWatcher::Type::kNonRecursive,
            base::BindRepeating(&Watcher::OnHostsFilePathWatcherChange,
                                base::Unretained(this)))) {
      LOG(ERROR) << "DNS hosts watch failed to start.";
      success = false;
    }
#endif  // !BUILDFLAG(IS_IOS)
    return success;
  }

 private:
#if !BUILDFLAG(IS_IOS)
  void OnHostsFilePathWatcherChange(const base::FilePath& path, bool error) {
    OnHostsChanged(!error);
  }
#endif  // !BUILDFLAG(IS_IOS)

  DnsConfigWatcher config_watcher_;
#if !BUILDFLAG(IS_IOS)
  base::FilePathWatcher hosts_watcher_;
#endif  // !BUILDFLAG(IS_IOS)
};

// A SerialWorker that uses libresolv to initialize res_state and converts
// it to DnsConfig.
class DnsConfigServicePosix::ConfigReader : public SerialWorker {
 public:
  explicit ConfigReader(DnsConfigServicePosix& service) : service_(&service) {
    // Allow execution on another thread; nothing thread-specific about
    // constructor.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~ConfigReader() override = default;

  ConfigReader(const ConfigReader&) = delete;
  ConfigReader& operator=(const ConfigReader&) = delete;

  std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override {
    return std::make_unique<WorkItem>();
  }

  bool OnWorkFinished(std::unique_ptr<SerialWorker::WorkItem>
                          serial_worker_work_item) override {
    DCHECK(serial_worker_work_item);
    DCHECK(!IsCancelled());

    WorkItem* work_item = static_cast<WorkItem*>(serial_worker_work_item.get());
    if (work_item->dns_config_.has_value()) {
      service_->OnConfigRead(std::move(work_item->dns_config_).value());
      return true;
    } else {
      LOG(WARNING) << "Failed to read DnsConfig.";
      return false;
    }
  }

 private:
  class WorkItem : public SerialWorker::WorkItem {
   public:
    void DoWork() override { dns_config_ = ReadDnsConfig(); }

   private:
    friend class ConfigReader;
    std::optional<DnsConfig> dns_config_;
  };

  // Raw pointer to owning DnsConfigService.
  const raw_ptr<DnsConfigServicePosix> service_;
};

DnsConfigServicePosix::DnsConfigServicePosix()
    : DnsConfigService(kFilePathHosts) {
  // Allow constructing on one thread and living on another.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DnsConfigServicePosix::~DnsConfigServicePosix() {
  if (config_reader_)
    config_reader_->Cancel();
}

void DnsConfigServicePosix::RefreshConfig() {
  InvalidateConfig();
  InvalidateHosts();
  ReadConfigNow();
  ReadHostsNow();
}

void DnsConfigServicePosix::ReadConfigNow() {
  if (!config_reader_)
    CreateReader();
  config_reader_->WorkNow();
}

bool DnsConfigServicePosix::StartWatching() {
  CreateReader();
  // TODO(szym): re-start watcher if that makes sense. http://crbug.com/116139
  watcher_ = std::make_unique<Watcher>(*this);
  return watcher_->Watch();
}

void DnsConfigServicePosix::CreateReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!config_reader_);
  config_reader_ = std::make_unique<ConfigReader>(*this);
}

std::optional<DnsConfig> ConvertResStateToDnsConfig(
    const struct __res_state& res) {
  DnsConfig dns_config;
  dns_config.unhandled_options = false;

  if (!(res.options & RES_INIT))
    return std::nullopt;

  std::optional<std::vector<IPEndPoint>> nameservers = GetNameservers(res);
  if (!nameservers)
    return std::nullopt;

  dns_config.nameservers = std::move(*nameservers);
  dns_config.search.clear();
  for (int i = 0; (i < MAXDNSRCH) && res.dnsrch[i]; ++i) {
    dns_config.search.emplace_back(res.dnsrch[i]);
  }

  dns_config.ndots = res.ndots;
  dns_config.fallback_period = base::Seconds(res.retrans);
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
    return std::nullopt;

  // If any name server is 0.0.0.0, assume the configuration is invalid.
  // TODO(szym): Measure how often this happens. http://crbug.com/125599
  for (const IPEndPoint& nameserver : dns_config.nameservers) {
    if (nameserver.address().IsZero())
      return std::nullopt;
  }
  return dns_config;
}

}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  // DnsConfigService on iOS doesn't watch the config so its result can become
  // inaccurate at any time.  Disable it to prevent promulgation of inaccurate
  // DnsConfigs.
#if BUILDFLAG(IS_IOS)
  return nullptr;
#else   // BUILDFLAG(IS_IOS)
  return std::make_unique<internal::DnsConfigServicePosix>();
#endif  // BUILDFLAG(IS_IOS)
}

}  // namespace net
