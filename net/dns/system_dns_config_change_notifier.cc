// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/system_dns_config_change_notifier.h"

#include <map>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "net/dns/dns_config_service.h"

namespace net {

namespace {

// Internal information and handling for a registered Observer. Handles
// posting to and DCHECKing the correct sequence for the Observer.
class WrappedObserver {
 public:
  explicit WrappedObserver(SystemDnsConfigChangeNotifier::Observer* observer)
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        observer_(observer) {}

  WrappedObserver(const WrappedObserver&) = delete;
  WrappedObserver& operator=(const WrappedObserver&) = delete;

  ~WrappedObserver() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  void OnNotifyThreadsafe(std::optional<DnsConfig> config) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WrappedObserver::OnNotify,
                       weak_ptr_factory_.GetWeakPtr(), std::move(config)));
  }

  void OnNotify(std::optional<DnsConfig> config) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!config || config.value().IsValid());

    observer_->OnSystemDnsConfigChanged(std::move(config));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const raw_ptr<SystemDnsConfigChangeNotifier::Observer> observer_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WrappedObserver> weak_ptr_factory_{this};
};

}  // namespace

// Internal core to be destroyed via base::OnTaskRunnerDeleter to ensure
// sequence safety.
class SystemDnsConfigChangeNotifier::Core {
 public:
  Core(scoped_refptr<base::SequencedTaskRunner> task_runner,
       std::unique_ptr<DnsConfigService> dns_config_service)
      : task_runner_(std::move(task_runner)) {
    DCHECK(task_runner_);
    DCHECK(dns_config_service);

    DETACH_FROM_SEQUENCE(sequence_checker_);

    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&Core::SetAndStartDnsConfigService,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(dns_config_service)));
  }

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(wrapped_observers_.empty());
  }

  void AddObserver(Observer* observer) {
    // Create wrapped observer outside locking in case construction requires
    // complex side effects.
    auto wrapped_observer = std::make_unique<WrappedObserver>(observer);

    {
      base::AutoLock lock(lock_);

      if (config_) {
        // Even though this is the same sequence as the observer, use the
        // threadsafe OnNotify to post the notification for both lock and
        // reentrancy safety.
        wrapped_observer->OnNotifyThreadsafe(config_);
      }

      DCHECK_EQ(0u, wrapped_observers_.count(observer));
      wrapped_observers_.emplace(observer, std::move(wrapped_observer));
    }
  }

  void RemoveObserver(Observer* observer) {
    // Destroy wrapped observer outside locking in case destruction requires
    // complex side effects.
    std::unique_ptr<WrappedObserver> removed_wrapped_observer;

    {
      base::AutoLock lock(lock_);
      auto it = wrapped_observers_.find(observer);
      CHECK(it != wrapped_observers_.end(), base::NotFatalUntil::M130);
      removed_wrapped_observer = std::move(it->second);
      wrapped_observers_.erase(it);
    }
  }

  void RefreshConfig() {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&Core::TriggerRefreshConfig,
                                          weak_ptr_factory_.GetWeakPtr()));
  }

  void SetDnsConfigServiceForTesting(
      std::unique_ptr<DnsConfigService> dns_config_service,
      base::OnceClosure done_cb) {
    DCHECK(dns_config_service);
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&Core::SetAndStartDnsConfigService,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(dns_config_service)));
    if (done_cb) {
      task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&Core::TriggerRefreshConfig,
                         weak_ptr_factory_.GetWeakPtr()),
          std::move(done_cb));
    }
  }

 private:
  void SetAndStartDnsConfigService(
      std::unique_ptr<DnsConfigService> dns_config_service) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    dns_config_service_ = std::move(dns_config_service);
    dns_config_service_->WatchConfig(base::BindRepeating(
        &Core::OnConfigChanged, weak_ptr_factory_.GetWeakPtr()));
  }

  void OnConfigChanged(const DnsConfig& config) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::AutoLock lock(lock_);

    // |config_| is |std::nullopt| if most recent config was invalid (or no
    // valid config has yet been read), so convert |config| to a similar form
    // before comparing for change.
    std::optional<DnsConfig> new_config;
    if (config.IsValid())
      new_config = config;

    if (config_ == new_config)
      return;

    config_ = std::move(new_config);

    for (auto& wrapped_observer : wrapped_observers_) {
      wrapped_observer.second->OnNotifyThreadsafe(config_);
    }
  }

  void TriggerRefreshConfig() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    dns_config_service_->RefreshConfig();
  }

  // Fields that may be accessed from any sequence. Must protect access using
  // |lock_|.
  mutable base::Lock lock_;
  // Only stores valid configs. |std::nullopt| if most recent config was
  // invalid (or no valid config has yet been read).
  std::optional<DnsConfig> config_;
  std::map<Observer*, std::unique_ptr<WrappedObserver>> wrapped_observers_;

  // Fields valid only on |task_runner_|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<DnsConfigService> dns_config_service_;
  base::WeakPtrFactory<Core> weak_ptr_factory_{this};
};

SystemDnsConfigChangeNotifier::SystemDnsConfigChangeNotifier()
    : SystemDnsConfigChangeNotifier(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          DnsConfigService::CreateSystemService()) {}

SystemDnsConfigChangeNotifier::SystemDnsConfigChangeNotifier(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<DnsConfigService> dns_config_service)
    : core_(nullptr, base::OnTaskRunnerDeleter(task_runner)) {
  if (dns_config_service)
    core_.reset(new Core(task_runner, std::move(dns_config_service)));
}

SystemDnsConfigChangeNotifier::~SystemDnsConfigChangeNotifier() = default;

void SystemDnsConfigChangeNotifier::AddObserver(Observer* observer) {
  if (core_)
    core_->AddObserver(observer);
}

void SystemDnsConfigChangeNotifier::RemoveObserver(Observer* observer) {
  if (core_)
    core_->RemoveObserver(observer);
}

void SystemDnsConfigChangeNotifier::RefreshConfig() {
  if (core_)
    core_->RefreshConfig();
}

void SystemDnsConfigChangeNotifier::SetDnsConfigServiceForTesting(
    std::unique_ptr<DnsConfigService> dns_config_service,
    base::OnceClosure done_cb) {
  DCHECK(core_);
  DCHECK(dns_config_service);

  core_->SetDnsConfigServiceForTesting(  // IN-TEST
      std::move(dns_config_service), std::move(done_cb));
}

}  // namespace net
