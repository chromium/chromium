// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_config_service_linux.h"

#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config.h"
#include "net/dns/nsswitch_reader.h"
#include "net/dns/public/resolv_reader.h"
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

std::optional<DnsConfig> ConvertResStateToDnsConfig(
    const struct __res_state& res) {
  std::optional<std::vector<net::IPEndPoint>> nameservers = GetNameservers(res);
  DnsConfig dns_config;
  dns_config.unhandled_options = false;

  if (!nameservers.has_value())
    return std::nullopt;

  // Expected to be validated by GetNameservers()
  DCHECK(res.options & RES_INIT);

  dns_config.nameservers = std::move(nameservers.value());
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
  for (const IPEndPoint& nameserver : dns_config.nameservers) {
    if (nameserver.address().IsZero())
      return std::nullopt;
  }
  return dns_config;
}

// Helper to add the effective result of `action` to `in_out_parsed_behavior`.
// Returns false if `action` results in inconsistent behavior (setting an action
// for a status that already has a different action).
bool SetActionBehavior(const NsswitchReader::ServiceAction& action,
                       std::map<NsswitchReader::Status, NsswitchReader::Action>&
                           in_out_parsed_behavior) {
  if (action.negated) {
    for (NsswitchReader::Status status :
         {NsswitchReader::Status::kSuccess, NsswitchReader::Status::kNotFound,
          NsswitchReader::Status::kUnavailable,
          NsswitchReader::Status::kTryAgain}) {
      if (status != action.status) {
        NsswitchReader::ServiceAction effective_action = {
            /*negated=*/false, status, action.action};
        if (!SetActionBehavior(effective_action, in_out_parsed_behavior))
          return false;
      }
    }
  } else {
    if (in_out_parsed_behavior.count(action.status) >= 1 &&
        in_out_parsed_behavior[action.status] != action.action) {
      return false;
    }
    in_out_parsed_behavior[action.status] = action.action;
  }

  return true;
}

// Helper to determine if `actions` match `expected_actions`, meaning `actions`
// contains no unknown statuses or actions and for every expectation set in
// `expected_actions`, the expected action matches the effective result from
// `actions`.
bool AreActionsCompatible(
    const std::vector<NsswitchReader::ServiceAction>& actions,
    const std::map<NsswitchReader::Status, NsswitchReader::Action>
        expected_actions) {
  std::map<NsswitchReader::Status, NsswitchReader::Action> parsed_behavior;

  for (const NsswitchReader::ServiceAction& action : actions) {
    if (action.status == NsswitchReader::Status::kUnknown ||
        action.action == NsswitchReader::Action::kUnknown) {
      return false;
    }

    if (!SetActionBehavior(action, parsed_behavior))
      return false;
  }

  // Default behavior if not configured.
  if (parsed_behavior.count(NsswitchReader::Status::kSuccess) == 0)
    parsed_behavior[NsswitchReader::Status::kSuccess] =
        NsswitchReader::Action::kReturn;
  if (parsed_behavior.count(NsswitchReader::Status::kNotFound) == 0)
    parsed_behavior[NsswitchReader::Status::kNotFound] =
        NsswitchReader::Action::kContinue;
  if (parsed_behavior.count(NsswitchReader::Status::kUnavailable) == 0)
    parsed_behavior[NsswitchReader::Status::kUnavailable] =
        NsswitchReader::Action::kContinue;
  if (parsed_behavior.count(NsswitchReader::Status::kTryAgain) == 0)
    parsed_behavior[NsswitchReader::Status::kTryAgain] =
        NsswitchReader::Action::kContinue;

  for (const std::pair<const NsswitchReader::Status, NsswitchReader::Action>&
           expected : expected_actions) {
    if (parsed_behavior[expected.first] != expected.second)
      return false;
  }

  return true;
}

// These values are emitted in metrics. Entries should not be renumbered and
// numeric values should never be reused. (See NsswitchIncompatibleReason in
// tools/metrics/histograms/enums.xml.)
enum class IncompatibleNsswitchReason {
  kFilesMissing = 0,
  kMultipleFiles = 1,
  kBadFilesActions = 2,
  kDnsMissing = 3,
  kBadDnsActions = 4,
  kBadMdnsMinimalActions = 5,
  kBadOtherServiceActions = 6,
  kUnknownService = 7,
  kIncompatibleService = 8,
  kMaxValue = kIncompatibleService
};

void RecordIncompatibleNsswitchReason(
    IncompatibleNsswitchReason reason,
    std::optional<NsswitchReader::Service> service_token) {
  if (service_token) {
    base::UmaHistogramEnumeration(
        "Net.DNS.DnsConfig.Nsswitch.IncompatibleService",
        service_token.value());
  }
}

bool IsNsswitchConfigCompatible(
    const std::vector<NsswitchReader::ServiceSpecification>& nsswitch_hosts) {
  bool files_found = false;
  for (const NsswitchReader::ServiceSpecification& specification :
       nsswitch_hosts) {
    switch (specification.service) {
      case NsswitchReader::Service::kUnknown:
        RecordIncompatibleNsswitchReason(
            IncompatibleNsswitchReason::kUnknownService, specification.service);
        return false;

      case NsswitchReader::Service::kFiles:
        if (files_found) {
          RecordIncompatibleNsswitchReason(
              IncompatibleNsswitchReason::kMultipleFiles,
              specification.service);
          return false;
        }
        files_found = true;
        // Chrome will use the result on HOSTS hit and otherwise continue to
        // DNS. `kFiles` entries must match that behavior to be compatible.
        if (!AreActionsCompatible(specification.actions,
                                  {{NsswitchReader::Status::kSuccess,
                                    NsswitchReader::Action::kReturn},
                                   {NsswitchReader::Status::kNotFound,
                                    NsswitchReader::Action::kContinue},
                                   {NsswitchReader::Status::kUnavailable,
                                    NsswitchReader::Action::kContinue},
                                   {NsswitchReader::Status::kTryAgain,
                                    NsswitchReader::Action::kContinue}})) {
          RecordIncompatibleNsswitchReason(
              IncompatibleNsswitchReason::kBadFilesActions,
              specification.service);
          return false;
        }
        break;

      case NsswitchReader::Service::kDns:
        if (!files_found) {
          RecordIncompatibleNsswitchReason(
              IncompatibleNsswitchReason::kFilesMissing,
              /*service_token=*/std::nullopt);
          return false;
        }
        // Chrome will always stop if DNS finds a result or will otherwise
        // fallback to the system resolver (and get whatever behavior is
        // configured in nsswitch.conf), so the only compatibility requirement
        // is that `kDns` entries are configured to return on success.
        if (!AreActionsCompatible(specification.actions,
                                  {{NsswitchReader::Status::kSuccess,
                                    NsswitchReader::Action::kReturn}})) {
          RecordIncompatibleNsswitchReason(
              IncompatibleNsswitchReason::kBadDnsActions,
              specification.service);
          return false;
        }

        // Ignore any entries after `kDns` because Chrome will fallback to the
        // system resolver if a result was not found in DNS.
        return true;

      case NsswitchReader::Service::kMdns:
      case NsswitchReader::Service::kMdns4:
      case NsswitchReader::Service::kMdns6:
      case NsswitchReader::Service::kResolve:
      case NsswitchReader::Service::kNis:
        RecordIncompatibleNsswitchReason(
            IncompatibleNsswitchReason::kIncompatibleService,
            specification.service);
        return false;

      case NsswitchReader::Service::kMdnsMinimal:
      case NsswitchReader::Service::kMdns4Minimal:
      case NsswitchReader::Service::kMdns6Minimal:
        // Always compatible as long as `kUnavailable` is `kContinue` because
        // the service is expected to always result in `kUnavailable` for any
        // names Chrome would attempt to resolve (non-*.local names because
        // Chrome always delegates *.local names to the system resolver).
        if (!AreActionsCompatible(specification.actions,
                                  {{NsswitchReader::Status::kUnavailable,
                                    NsswitchReader::Action::kContinue}})) {
          RecordIncompatibleNsswitchReason(
              IncompatibleNsswitchReason::kBadMdnsMinimalActions,
              specification.service);
          return false;
        }
        break;

      case NsswitchReader::Service::kMyHostname:
        // Similar enough to Chrome behavior (or unlikely to matter for Chrome
        // resolutions) to be considered compatible unless the actions do
        // something very weird to skip remaining services without a result.
        if (!AreActionsCompatible(specification.actions,
                                  {{NsswitchReader::Status::kNotFound,
                                    NsswitchReader::Action::kContinue},
                                   {NsswitchReader::Status::kUnavailable,
                                    NsswitchReader::Action::kContinue},
                                   {NsswitchReader::Status::kTryAgain,
                                    NsswitchReader::Action::kContinue}})) {
          RecordIncompatibleNsswitchReason(
              IncompatibleNsswitchReason::kBadOtherServiceActions,
              specification.service);
          return false;
        }
        break;
    }
  }

  RecordIncompatibleNsswitchReason(IncompatibleNsswitchReason::kDnsMissing,
                                   /*service_token=*/std::nullopt);
  return false;
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
            base::BindRepeating(&Watcher::OnResolvFilePathWatcherChange,
                                base::Unretained(this)))) {
      LOG(ERROR) << "DNS config (resolv.conf) watch failed to start.";
      success = false;
    }

    if (!nsswitch_watcher_.Watch(
            base::FilePath(kFilePathNsswitch),
            base::FilePathWatcher::Type::kNonRecursive,
            base::BindRepeating(&Watcher::OnNsswitchFilePathWatcherChange,
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
  void OnResolvFilePathWatcherChange(const base::FilePath& path, bool error) {
    OnConfigChanged(!error);
  }

  void OnNsswitchFilePathWatcherChange(const base::FilePath& path, bool error) {
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
        work_item_(std::make_unique<WorkItem>(std::move(resolv_reader),
                                              std::move(nsswitch_reader))) {
    // Allow execution on another thread; nothing thread-specific about
    // constructor.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~ConfigReader() override = default;

  ConfigReader(const ConfigReader&) = delete;
  ConfigReader& operator=(const ConfigReader&) = delete;

  std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override {
    // Reuse same `WorkItem` to allow reuse of contained reader objects.
    DCHECK(work_item_);
    return std::move(work_item_);
  }

  bool OnWorkFinished(std::unique_ptr<SerialWorker::WorkItem>
                          serial_worker_work_item) override {
    DCHECK(serial_worker_work_item);
    DCHECK(!work_item_);
    DCHECK(!IsCancelled());

    work_item_.reset(static_cast<WorkItem*>(serial_worker_work_item.release()));
    if (work_item_->dns_config_.has_value()) {
      service_->OnConfigRead(std::move(work_item_->dns_config_).value());
      return true;
    } else {
      LOG(WARNING) << "Failed to read DnsConfig.";
      return false;
    }
  }

 private:
  class WorkItem : public SerialWorker::WorkItem {
   public:
    WorkItem(std::unique_ptr<ResolvReader> resolv_reader,
             std::unique_ptr<NsswitchReader> nsswitch_reader)
        : resolv_reader_(std::move(resolv_reader)),
          nsswitch_reader_(std::move(nsswitch_reader)) {
      DCHECK(resolv_reader_);
      DCHECK(nsswitch_reader_);
    }

    void DoWork() override {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);

      {
        std::unique_ptr<ScopedResState> res = resolv_reader_->GetResState();
        if (res) {
          dns_config_ = ConvertResStateToDnsConfig(res->state());
        }
      }

      if (!dns_config_.has_value())
        return;
      base::UmaHistogramBoolean("Net.DNS.DnsConfig.Resolv.Compatible",
                                !dns_config_->unhandled_options);

      // Override `fallback_period` value to match default setting on
      // Windows.
      dns_config_->fallback_period = kDnsDefaultFallbackPeriod;

      if (dns_config_ && !dns_config_->unhandled_options) {
        std::vector<NsswitchReader::ServiceSpecification> nsswitch_hosts =
            nsswitch_reader_->ReadAndParseHosts();
        dns_config_->unhandled_options =
            !IsNsswitchConfigCompatible(nsswitch_hosts);
        base::UmaHistogramBoolean("Net.DNS.DnsConfig.Nsswitch.Compatible",
                                  !dns_config_->unhandled_options);
      }
    }

   private:
    friend class ConfigReader;
    std::optional<DnsConfig> dns_config_;
    std::unique_ptr<ResolvReader> resolv_reader_;
    std::unique_ptr<NsswitchReader> nsswitch_reader_;
  };

  // Raw pointer to owning DnsConfigService.
  const raw_ptr<DnsConfigServiceLinux> service_;

  // Null while the `WorkItem` is running on the `ThreadPool`.
  std::unique_ptr<WorkItem> work_item_;
};

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
  config_reader_ = std::make_unique<ConfigReader>(
      *this, std::move(resolv_reader_), std::move(nsswitch_reader_));
}

}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return std::make_unique<internal::DnsConfigServiceLinux>();
}

}  // namespace net
