// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/network_service_test_helper.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/process/process.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_host_resolver.h"
#include "content/utility/services.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/address_map_linux.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/network/cookie_manager.h"
#include "services/network/disk_cache/mojo_backend_file_operations_factory.h"
#include "services/network/host_resolver.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "services/network/sct_auditing/sct_auditing_reporter.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/test/android/url_utils.h"
#endif

namespace content {
namespace {

#ifndef STATIC_ASSERT_ENUM
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
#endif

STATIC_ASSERT_ENUM(network::mojom::ResolverType::kResolverTypeFail,
                   net::RuleBasedHostResolverProc::Rule::kResolverTypeFail);
STATIC_ASSERT_ENUM(network::mojom::ResolverType::kResolverTypeSystem,
                   net::RuleBasedHostResolverProc::Rule::kResolverTypeSystem);
STATIC_ASSERT_ENUM(
    network::mojom::ResolverType::kResolverTypeIPLiteral,
    net::RuleBasedHostResolverProc::Rule::kResolverTypeIPLiteral);

void CrashResolveHost(const std::string& host_to_crash,
                      const std::string& host) {
  if (host_to_crash == host)
    base::Process::TerminateCurrentProcessImmediately(1);
}

class SimpleCacheEntry : public network::mojom::SimpleCacheEntry {
 public:
  explicit SimpleCacheEntry(disk_cache::ScopedEntryPtr entry)
      : entry_(std::move(entry)) {}
  ~SimpleCacheEntry() override = default;

  void WriteData(int32_t index,
                 int32_t offset,
                 const std::vector<uint8_t>& data,
                 bool truncate,
                 WriteDataCallback callback) override {
    if (!entry_) {
      std::move(callback).Run(net::ERR_FAILED);
      return;
    }
    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<WriteDataCallback>>();
    callback_holder->data = std::move(callback);

    auto data_to_pass =
        base::MakeRefCounted<net::IOBufferWithSize>(data.size());
    memcpy(data_to_pass->data(), data.data(), data.size());
    int rv = entry_->WriteData(index, offset, data_to_pass.get(), data.size(),
                               base::BindOnce(&SimpleCacheEntry::OnDataWritten,
                                              weak_factory_.GetWeakPtr(),
                                              data_to_pass, callback_holder),
                               truncate);
    if (rv == net::ERR_IO_PENDING) {
      return;
    }
    OnDataWritten(std::move(data_to_pass), std::move(callback_holder), rv);
  }

  void ReadData(int32_t index,
                int32_t offset,
                uint32_t length,
                ReadDataCallback callback) override {
    if (!entry_) {
      std::move(callback).Run(/*data=*/{}, net::ERR_FAILED);
      return;
    }

    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<ReadDataCallback>>();
    callback_holder->data = std::move(callback);

    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(length);
    int rv = entry_->ReadData(
        index, offset, buffer.get(), length,
        base::BindOnce(&SimpleCacheEntry::OnDataRead,
                       weak_factory_.GetWeakPtr(), buffer, callback_holder));
    if (rv == net::ERR_IO_PENDING) {
      return;
    }
    OnDataRead(std::move(buffer), std::move(callback_holder), rv);
  }

  void WriteSparseData(int32_t offset,
                       const std::vector<uint8_t>& data,
                       WriteDataCallback callback) override {
    if (!entry_) {
      std::move(callback).Run(net::ERR_FAILED);
      return;
    }
    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<WriteDataCallback>>();
    callback_holder->data = std::move(callback);

    auto data_to_pass =
        base::MakeRefCounted<net::IOBufferWithSize>(data.size());
    memcpy(data_to_pass->data(), data.data(), data.size());
    int rv =
        entry_->WriteSparseData(offset, data_to_pass.get(), data.size(),
                                base::BindOnce(&SimpleCacheEntry::OnDataWritten,
                                               weak_factory_.GetWeakPtr(),
                                               data_to_pass, callback_holder));
    if (rv == net::ERR_IO_PENDING) {
      return;
    }
    OnDataWritten(std::move(data_to_pass), std::move(callback_holder), rv);
  }

  void ReadSparseData(int32_t offset,
                      uint32_t length,
                      ReadDataCallback callback) override {
    if (!entry_) {
      std::move(callback).Run(/*data=*/{}, net::ERR_FAILED);
      return;
    }

    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<ReadDataCallback>>();
    callback_holder->data = std::move(callback);

    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(length);
    int rv = entry_->ReadSparseData(
        offset, buffer.get(), length,
        base::BindOnce(&SimpleCacheEntry::OnDataRead,
                       weak_factory_.GetWeakPtr(), buffer, callback_holder));
    if (rv == net::ERR_IO_PENDING) {
      return;
    }
    OnDataRead(std::move(buffer), std::move(callback_holder), rv);
  }

  void Close(CloseCallback callback) override {
    entry_.reset();
    std::move(callback).Run();
  }

 private:
  void OnDataWritten(
      scoped_refptr<net::IOBuffer> buffer,
      scoped_refptr<base::RefCountedData<WriteDataCallback>> callback_holder,
      int result) {
    WriteDataCallback callback = std::move(callback_holder->data);
    std::move(callback).Run(result);
  }

  void OnDataRead(
      scoped_refptr<net::IOBuffer> buffer,
      scoped_refptr<base::RefCountedData<ReadDataCallback>> callback_holder,
      int result) {
    ReadDataCallback callback = std::move(callback_holder->data);
    if (result < 0) {
      std::move(callback).Run(/*data=*/{}, result);
      return;
    }
    std::vector<uint8_t> data(result);
    memcpy(data.data(), buffer->data(), result);
    std::move(callback).Run(data, result);
  }

  disk_cache::ScopedEntryPtr entry_;
  base::WeakPtrFactory<SimpleCacheEntry> weak_factory_{this};
};

class SimpleCacheEntryEnumerator final
    : public network::mojom::SimpleCacheEntryEnumerator {
 public:
  using GetNextCallbackHolder = base::RefCountedData<GetNextCallback>;
  explicit SimpleCacheEntryEnumerator(
      std::unique_ptr<disk_cache::Backend::Iterator> iter)
      : iter_(std::move(iter)) {}
  ~SimpleCacheEntryEnumerator() override = default;

  void GetNext(GetNextCallback callback) override {
    DCHECK(!processing_);
    processing_ = true;
    auto callback_holder =
        base::MakeRefCounted<GetNextCallbackHolder>(std::move(callback));
    disk_cache::EntryResult result = iter_->OpenNextEntry(
        base::BindOnce(&SimpleCacheEntryEnumerator::OnEntryOpened,
                       weak_factory_.GetWeakPtr(), callback_holder));
    if (result.net_error() == net::ERR_IO_PENDING) {
      return;
    }

    OnEntryOpened(std::move(callback_holder), std::move(result));
  }

 private:
  void OnEntryOpened(scoped_refptr<GetNextCallbackHolder> callback_holder,
                     disk_cache::EntryResult result) {
    DCHECK(processing_);
    processing_ = false;
    auto result_to_pass = network::mojom::SimpleCacheOpenEntryResult::New();
    result_to_pass->error = result.net_error();

    auto callback = std::move(callback_holder->data);
    DCHECK(callback);
    if (result.net_error() != net::OK) {
      std::move(callback).Run(std::move(result_to_pass));
      return;
    }

    disk_cache::ScopedEntryPtr entry(result.ReleaseEntry());
    result_to_pass->key = entry->GetKey();

    mojo::PendingRemote<network::mojom::SimpleCacheEntry> remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SimpleCacheEntry>(std::move(entry)),
        result_to_pass->entry.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(result_to_pass));
  }

  std::unique_ptr<disk_cache::Backend::Iterator> iter_;
  bool processing_ = false;

  base::WeakPtrFactory<SimpleCacheEntryEnumerator> weak_factory_{this};
};

class SimpleCache : public network::mojom::SimpleCache {
 public:
  explicit SimpleCache(std::unique_ptr<disk_cache::Backend> backend)
      : backend_(std::move(backend)) {
    DCHECK(backend_);
  }
  ~SimpleCache() override = default;

  void CreateEntry(const std::string& key,
                   CreateEntryCallback callback) override {
    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<CreateEntryCallback>>();
    callback_holder->data = std::move(callback);

    disk_cache::EntryResult result = backend_->CreateEntry(
        key, net::DEFAULT_PRIORITY,
        base::BindOnce(&SimpleCache::OnEntryCreated, weak_factory_.GetWeakPtr(),
                       callback_holder));
    if (result.net_error() == net::ERR_IO_PENDING) {
      return;
    }
    OnEntryCreated(std::move(callback_holder), std::move(result));
  }

  void OpenEntry(const std::string& key, OpenEntryCallback callback) override {
    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<OpenEntryCallback>>();
    callback_holder->data = std::move(callback);

    disk_cache::EntryResult result = backend_->OpenEntry(
        key, net::DEFAULT_PRIORITY,
        base::BindOnce(&SimpleCache::OnEntryOpend, weak_factory_.GetWeakPtr(),
                       callback_holder));
    if (result.net_error() == net::ERR_IO_PENDING) {
      return;
    }
    OnEntryOpend(std::move(callback_holder), std::move(result));
  }

  void DoomEntry(const std::string& key, DoomEntryCallback callback) override {
    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<DoomEntryCallback>>();
    callback_holder->data = std::move(callback);
    int rv = backend_->DoomEntry(
        key, net::IDLE,
        base::BindOnce(&SimpleCache::OnEntryDoomed, weak_factory_.GetWeakPtr(),
                       callback_holder));

    if (rv == net::ERR_IO_PENDING) {
      return;
    }
    OnEntryDoomed(std::move(callback_holder), rv);
  }

  void DoomAllEntries(DoomAllEntriesCallback callback) override {
    auto callback_holder =
        base::MakeRefCounted<base::RefCountedData<DoomAllEntriesCallback>>(
            std::move(callback));
    int rv = backend_->DoomAllEntries(
        base::BindOnce(&SimpleCache::OnAllEntriesDoomed,
                       weak_factory_.GetWeakPtr(), callback_holder));

    if (rv == net::ERR_IO_PENDING) {
      return;
    }
    OnAllEntriesDoomed(std::move(callback_holder), rv);
  }

  void EnumerateEntries(
      mojo::PendingReceiver<network::mojom::SimpleCacheEntryEnumerator>
          pending_receiver) override {
    mojo::MakeSelfOwnedReceiver(std::make_unique<SimpleCacheEntryEnumerator>(
                                    backend_->CreateIterator()),
                                std::move(pending_receiver));
  }

  void Detach(DetachCallback callback) override {
    backend_ = nullptr;
    disk_cache::FlushCacheThreadAsynchronouslyForTesting(std::move(callback));
  }

 private:
  void OnEntryCreated(
      scoped_refptr<base::RefCountedData<CreateEntryCallback>> callback_holder,
      disk_cache::EntryResult result) {
    CreateEntryCallback callback = std::move(callback_holder->data);
    if (result.net_error() != net::OK) {
      std::move(callback).Run(mojo::NullRemote(), result.net_error());
      return;
    }
    disk_cache::ScopedEntryPtr entry(result.ReleaseEntry());

    mojo::PendingRemote<network::mojom::SimpleCacheEntry> remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SimpleCacheEntry>(std::move(entry)),
        remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote), net::OK);
  }

  void OnEntryOpend(
      scoped_refptr<base::RefCountedData<OpenEntryCallback>> callback_holder,
      disk_cache::EntryResult result) {
    OpenEntryCallback callback = std::move(callback_holder->data);
    if (result.net_error() != net::OK) {
      std::move(callback).Run(mojo::NullRemote(), result.net_error());
      return;
    }
    disk_cache::ScopedEntryPtr entry(result.ReleaseEntry());

    mojo::PendingRemote<network::mojom::SimpleCacheEntry> remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SimpleCacheEntry>(std::move(entry)),
        remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote), net::OK);
  }

  void OnEntryDoomed(
      scoped_refptr<base::RefCountedData<DoomEntryCallback>> callback_holder,
      int result) {
    DoomEntryCallback callback = std::move(callback_holder->data);
    std::move(callback).Run(result);
  }

  void OnAllEntriesDoomed(
      scoped_refptr<base::RefCountedData<DoomAllEntriesCallback>>
          callback_holder,
      int result) {
    DoomAllEntriesCallback callback = std::move(callback_holder->data);
    std::move(callback).Run(result);
  }

  std::unique_ptr<disk_cache::Backend> backend_;

  base::WeakPtrFactory<SimpleCache> weak_factory_{this};
};

}  // namespace

class NetworkServiceTestHelper::NetworkServiceTestImpl
    : public network::mojom::NetworkServiceTest,
      public base::CurrentThread::DestructionObserver {
 public:
  NetworkServiceTestImpl() : test_host_resolver_(new TestHostResolver()) {
    memory_pressure_listener_.emplace(
        FROM_HERE, base::DoNothing(),
        base::BindRepeating(
            &NetworkServiceTestHelper::NetworkServiceTestImpl::OnMemoryPressure,
            weak_factory_.GetWeakPtr()));

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUseMockCertVerifierForTesting)) {
      mock_cert_verifier_ = std::make_unique<net::MockCertVerifier>();
      network::NetworkContext::SetCertVerifierForTesting(
          mock_cert_verifier_.get());

      // The default result may be set using a command line flag.
      std::string default_result =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kMockCertVerifierDefaultResultForTesting);
      int default_result_int = 0;
      if (!default_result.empty() &&
          base::StringToInt(default_result, &default_result_int)) {
        mock_cert_verifier_->set_default_result(default_result_int);
      }
    }
  }

  NetworkServiceTestImpl(const NetworkServiceTestImpl&) = delete;
  NetworkServiceTestImpl& operator=(const NetworkServiceTestImpl&) = delete;

  ~NetworkServiceTestImpl() override {
    network::NetworkContext::SetCertVerifierForTesting(nullptr);
  }

  // network::mojom::NetworkServiceTest:
  void AddRules(std::vector<network::mojom::RulePtr> rules,
                AddRulesCallback callback) override {
    // test_host_resolver_ may be empty if
    // SetAllowNetworkAccessToHostResolutions was invoked.
    DCHECK(test_host_resolver_);
    auto* host_resolver = test_host_resolver_->host_resolver();
    for (const auto& rule : rules) {
      switch (rule->resolver_type) {
        case network::mojom::ResolverType::kResolverTypeFail:
          host_resolver->AddSimulatedFailure(rule->host_pattern);
          break;
        case network::mojom::ResolverType::kResolverTypeFailTimeout:
          host_resolver->AddSimulatedTimeoutFailure(rule->host_pattern);
          break;
        case network::mojom::ResolverType::kResolverTypeIPLiteral: {
          net::IPAddress ip_address;
          DCHECK(ip_address.AssignFromIPLiteral(rule->replacement));
          host_resolver->AddRuleWithFlags(rule->host_pattern, rule->replacement,
                                          rule->host_resolver_flags,
                                          rule->dns_aliases);
          break;
        }
        case network::mojom::ResolverType::kResolverTypeDirectLookup:
          host_resolver->AllowDirectLookup(rule->host_pattern);
          break;
        default:
          host_resolver->AddRuleWithFlags(rule->host_pattern, rule->replacement,
                                          rule->host_resolver_flags,
                                          rule->dns_aliases);
          break;
      }
    }
    std::move(callback).Run();
  }

  void SimulateNetworkChange(network::mojom::ConnectionType type,
                             SimulateNetworkChangeCallback callback) override {
    DCHECK(!net::NetworkChangeNotifier::CreateIfNeeded());
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::ConnectionType(type));
    std::move(callback).Run();
  }

  void SimulateNetworkQualityChange(
      net::EffectiveConnectionType type,
      SimulateNetworkChangeCallback callback) override {
    network::NetworkService::GetNetworkServiceForTesting()
        ->network_quality_estimator()
        ->SimulateNetworkQualityChangeForTesting(type);
    std::move(callback).Run();
  }

  void ForceNetworkQualityEstimatorReportWifiAsSlow2G(
      SimulateNetworkChangeCallback callback) override {
    network::NetworkService::GetNetworkServiceForTesting()
        ->network_quality_estimator()
        ->ForceReportWifiAsSlow2GForTesting();
    std::move(callback).Run();
  }

  void SimulateCrash() override {
    LOG(ERROR) << "Intentionally terminating current process to simulate"
                  " NetworkService crash for testing.";
    // Use |TerminateCurrentProcessImmediately()| instead of |CHECK()| to avoid
    // 'Fatal error' dialog on Windows debug.
    base::Process::TerminateCurrentProcessImmediately(1);
  }

  void MockCertVerifierSetDefaultResult(
      int32_t default_result,
      MockCertVerifierSetDefaultResultCallback callback) override {
    // TODO(crbug.com/40243688): Since testing/variations/
    // fieldtrial_testing_config.json changes the command line flags after
    // ContentBrowserTest::SetUpCommandLine() and NetworkServiceTest
    // instantiation, MockCertVerifierSetDefaultResult can be called without
    // `mock_cert_verifier_` initialization.
    // Actually since all mock cert verification tests call this function first,
    // we should remove the set up using the command line flags.
    if (!mock_cert_verifier_) {
      mock_cert_verifier_ = std::make_unique<net::MockCertVerifier>();
      network::NetworkContext::SetCertVerifierForTesting(
          mock_cert_verifier_.get());
    }
    mock_cert_verifier_->set_default_result(default_result);
    std::move(callback).Run();
  }

  void MockCertVerifierAddResultForCertAndHost(
      const scoped_refptr<net::X509Certificate>& cert,
      const std::string& host_pattern,
      const net::CertVerifyResult& verify_result,
      int32_t rv,
      MockCertVerifierAddResultForCertAndHostCallback callback) override {
    mock_cert_verifier_->AddResultForCertAndHost(cert, host_pattern,
                                                 verify_result, rv);
    std::move(callback).Run();
  }

  void SetTransportSecurityStateSource(
      uint16_t reporting_port,
      SetTransportSecurityStateSourceCallback callback) override {
    if (reporting_port) {
      transport_security_state_source_ =
          std::make_unique<net::ScopedTransportSecurityStateSource>(
              reporting_port);
    } else {
      transport_security_state_source_.reset();
    }
    std::move(callback).Run();
  }

  void SetAllowNetworkAccessToHostResolutions(
      SetAllowNetworkAccessToHostResolutionsCallback callback) override {
    DCHECK(!have_test_doh_servers_)
        << "Cannot allow network access when test DoH servers have been set.";
    test_host_resolver_.reset();
    std::move(callback).Run();
  }

  void ReplaceSystemDnsConfig(
      ReplaceSystemDnsConfigCallback callback) override {
    network::NetworkService::GetNetworkServiceForTesting()
        ->ReplaceSystemDnsConfigForTesting(std::move(callback));
  }

  void SetTestDohConfig(net::SecureDnsMode secure_dns_mode,
                        const net::DnsOverHttpsConfig& doh_config,
                        SetTestDohConfigCallback callback) override {
    DCHECK(test_host_resolver_)
        << "Network access for host resolutions must be disabled.";
    have_test_doh_servers_ = true;
    network::NetworkService::GetNetworkServiceForTesting()
        ->SetTestDohConfigForTesting(secure_dns_mode, doh_config);
    std::move(callback).Run();
  }

  void CrashOnResolveHost(const std::string& host) override {
    network::HostResolver::SetResolveHostCallbackForTesting(
        base::BindRepeating(CrashResolveHost, host));
  }

  void CrashOnGetCookieList() override {
    network::CookieManager::CrashOnGetCookieList();
  }

  void GetLatestMemoryPressureLevel(
      GetLatestMemoryPressureLevelCallback callback) override {
    std::move(callback).Run(latest_memory_pressure_level_);
  }

  void GetPeerToPeerConnectionsCountChange(
      GetPeerToPeerConnectionsCountChangeCallback callback) override {
    uint32_t count = network::NetworkService::GetNetworkServiceForTesting()
                         ->network_quality_estimator()
                         ->GetPeerToPeerConnectionsCountChange();

    std::move(callback).Run(count);
  }

  void SetSCTAuditingRetryDelay(
      std::optional<base::TimeDelta> delay,
      SetSCTAuditingRetryDelayCallback callback) override {
#if BUILDFLAG(IS_CT_SUPPORTED)
    network::SCTAuditingReporter::SetRetryDelayForTesting(delay);
#endif
    std::move(callback).Run();
  }

  void GetEnvironmentVariableValue(
      const std::string& name,
      GetEnvironmentVariableValueCallback callback) override {
    std::string value;
    base::Environment::Create()->GetVar(name, &value);
    std::move(callback).Run(value);
  }

  void Log(const std::string& message, LogCallback callback) override {
    LOG(ERROR) << message;
    std::move(callback).Run();
  }

  void ActivateFieldTrial(const std::string& field_trial_name) override {
    base::FieldTrialList::FindFullName(field_trial_name);
  }

  void BindReceiver(
      mojo::PendingReceiver<network::mojom::NetworkServiceTest> receiver) {
    receivers_.Add(this, std::move(receiver));
    if (!registered_as_destruction_observer_) {
      base::CurrentIOThread::Get()->AddDestructionObserver(this);
      registered_as_destruction_observer_ = true;
    }
  }

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    // Needs to be called on the IO thread.
    receivers_.Clear();
  }

  void OpenFile(const base::FilePath& path,
                base::OnceCallback<void(bool)> callback) override {
    base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);
    std::move(callback).Run(file.IsValid());
  }

  void EnumerateFiles(
      const base::FilePath& path,
      mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
          factory_remote,
      EnumerateFilesCallback callback) override {
    auto task_runner =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
    auto factory =
        base::MakeRefCounted<network::MojoBackendFileOperationsFactory>(
            std::move(factory_remote));
    auto ops = factory->Create(task_runner);

    using Entry = disk_cache::BackendFileOperations::FileEnumerationEntry;

    task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& path,
               std::unique_ptr<disk_cache::BackendFileOperations> ops) {
              base::ScopedAllowBaseSyncPrimitivesForTesting scope;
              auto enumerator = ops->EnumerateFiles(path);
              std::vector<Entry> entries;
              while (auto entry = enumerator->Next()) {
                entries.push_back(std::move(*entry));
              }
              return std::make_pair(entries, enumerator->HasError());
            },
            path, std::move(ops)),
        base::BindOnce(
            [](EnumerateFilesCallback callback,
               std::pair<std::vector<Entry>, bool> arg) {
              std::move(callback).Run(arg.first, arg.second);
            },
            std::move(callback)));
  }

  void CreateSimpleCache(
      mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
          factory,
      const base::FilePath& path,
      bool reset,
      CreateSimpleCacheCallback callback) override {
    const auto reset_mode = reset ? disk_cache::ResetHandling::kReset
                                  : disk_cache::ResetHandling::kResetOnError;
    disk_cache::BackendResult result = disk_cache::CreateCacheBackend(
        net::DISK_CACHE, net::CACHE_BACKEND_SIMPLE,
        base::MakeRefCounted<network::MojoBackendFileOperationsFactory>(
            std::move(factory)),
        path, 64 * 1024 * 1024, reset_mode, net::NetLog::Get(),
        base::BindOnce(&NetworkServiceTestImpl::OnCacheCreated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    DCHECK_EQ(result.net_error, net::ERR_IO_PENDING);
  }

  void MakeRequestToServer(network::TransferableSocket transferred,
                           const net::IPEndPoint& endpoint,
                           MakeRequestToServerCallback callback) override {
    std::unique_ptr<net::TCPSocket> socket =
        net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
    socket->AdoptConnectedSocket(transferred.TakeSocket(), endpoint);
    const std::string kRequest("GET / HTTP/1.0\r\n\r\n");
    auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kRequest);

    int rv = socket->Write(io_buffer.get(), io_buffer->size(),
                           base::DoNothing(), TRAFFIC_ANNOTATION_FOR_TESTS);
    // For purposes of tests, this IPC only supports sync Write calls.
    DCHECK_NE(net::ERR_IO_PENDING, rv);
    std::move(callback).Run(rv == static_cast<int>(kRequest.size()));
  }

  void ResolveOwnHostnameWithSystemDns(
      ResolveOwnHostnameWithSystemDnsCallback callback) override {
    std::unique_ptr<net::HostResolverSystemTask> system_task =
        net::HostResolverSystemTask::CreateForOwnHostname(
            net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED, 0);
    net::HostResolverSystemTask* system_task_ptr = system_task.get();
    auto forward_system_dns_results =
        [](std::unique_ptr<net::HostResolverSystemTask>,
           net::SystemDnsResultsCallback callback,
           const net::AddressList& addr_list, int os_error, int net_error) {
          std::move(callback).Run(addr_list, os_error, net_error);
        };
    auto results_cb = base::BindOnce(
        std::move(forward_system_dns_results), std::move(system_task),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            std::move(callback), net::AddressList(), 0, net::ERR_ABORTED));
    system_task_ptr->Start(std::move(results_cb));
  }

  void SetIPv6ProbeResult(bool success,
                          SetIPv6ProbeResultCallback callback) override {
    network::NetworkService::GetNetworkServiceForTesting()
        ->host_resolver_manager()
        ->SetLastIPv6ProbeResultForTesting(success);
    std::move(callback).Run();
  }

#if BUILDFLAG(IS_LINUX)
  void GetAddressMapCacheLinux(
      GetAddressMapCacheLinuxCallback callback) override {
    const net::AddressMapOwnerLinux* address_map_owner =
        net::NetworkChangeNotifier::GetAddressMapOwner();
    std::move(callback).Run(address_map_owner->GetAddressMap(),
                            address_map_owner->GetOnlineLinks());
  }
#endif  // BUILDFLAG(IS_LINUX)

  void AllowsGSSAPILibraryLoad(
      AllowsGSSAPILibraryLoadCallback callback) override {
    bool allow_gssapi_library_load;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    allow_gssapi_library_load =
        network::NetworkService::GetNetworkServiceForTesting()
            ->http_auth_dynamic_network_service_params_for_testing()
            ->allow_gssapi_library_load;
#else
    allow_gssapi_library_load = true;
#endif

    std::move(callback).Run(allow_gssapi_library_load);
  }

 private:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
    latest_memory_pressure_level_ = memory_pressure_level;
  }

  void OnCacheCreated(CreateSimpleCacheCallback callback,
                      disk_cache::BackendResult result) {
    std::unique_ptr<disk_cache::Backend> backend = std::move(result.backend);
    if (result.net_error != net::OK) {
      DCHECK(!backend);
      std::move(callback).Run(mojo::NullRemote());
      return;
    }
    DCHECK(backend);
    mojo::PendingRemote<network::mojom::SimpleCache> remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SimpleCache>(std::move(backend)),
        remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote));
  }

  void OnNetworkDataWritten(int rv) { write_result_ = rv; }

  bool registered_as_destruction_observer_ = false;
  bool have_test_doh_servers_ = false;
  mojo::ReceiverSet<network::mojom::NetworkServiceTest> receivers_;
  std::unique_ptr<TestHostResolver> test_host_resolver_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;
  std::unique_ptr<net::ScopedTransportSecurityStateSource>
      transport_security_state_source_;
  std::optional<base::MemoryPressureListener> memory_pressure_listener_;
  base::MemoryPressureListener::MemoryPressureLevel
      latest_memory_pressure_level_ =
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  int write_result_;
  std::unique_ptr<disk_cache::Backend> disk_cache_backend_;

  base::WeakPtrFactory<NetworkServiceTestImpl> weak_factory_{this};
};

NetworkServiceTestHelper::NetworkServiceTestHelper()
    : network_service_test_impl_(new NetworkServiceTestImpl) {
  static bool is_created = false;
  DCHECK(!is_created) << "NetworkServiceTestHelper shouldn't be created twice.";
  is_created = true;
}

NetworkServiceTestHelper::~NetworkServiceTestHelper() = default;

std::unique_ptr<NetworkServiceTestHelper> NetworkServiceTestHelper::Create() {
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUtilitySubType) == network::mojom::NetworkService::Name_) {
    std::unique_ptr<NetworkServiceTestHelper> helper(
        new NetworkServiceTestHelper());
    SetNetworkBinderCreationCallbackForTesting(
        base::BindOnce(&NetworkServiceTestHelper::RegisterNetworkBinders,
                       base::Unretained(helper.get())));
    return helper;
  }
  return nullptr;
}

void NetworkServiceTestHelper::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface(base::BindRepeating(
      [](NetworkServiceTestHelper* helper,
         mojo::PendingReceiver<network::mojom::NetworkServiceTest> receiver) {
        helper->network_service_test_impl_->BindReceiver(std::move(receiver));
      },
      base::Unretained(this)));
}

}  // namespace content
