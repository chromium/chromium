// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/network_service_test_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/process/process.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_host_resolver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/ip_address.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/test_data_directory.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/network/cookie_manager.h"
#include "services/network/disk_cache/mojo_backend_file_operations_factory.h"
#include "services/network/host_resolver.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "services/network/sct_auditing/sct_auditing_reporter.h"

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

class SimpleCache : public network::mojom::SimpleCache {
 public:
  explicit SimpleCache(std::unique_ptr<disk_cache::Backend> backend)
      : backend_(std::move(backend)) {}
  ~SimpleCache() override = default;

 private:
  std::unique_ptr<disk_cache::Backend> backend_;
};

}  // namespace

class NetworkServiceTestHelper::NetworkServiceTestImpl
    : public network::mojom::NetworkServiceTest,
      public base::CurrentThread::DestructionObserver {
 public:
  NetworkServiceTestImpl()
      : test_host_resolver_(new TestHostResolver()),
        memory_pressure_listener_(
            FROM_HERE,
            base::DoNothing(),
            base::BindRepeating(&NetworkServiceTestHelper::
                                    NetworkServiceTestImpl::OnMemoryPressure,
                                base::Unretained(this))) {
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

  void SetRequireCT(RequireCT required,
                    SetRequireCTCallback callback) override {
    net::TransportSecurityState::SetRequireCTForTesting(
        required == NetworkServiceTest::RequireCT::REQUIRE);
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
        ->ReplaceSystemDnsConfigForTesting();
    std::move(callback).Run();
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
      absl::optional<base::TimeDelta> delay,
      SetSCTAuditingRetryDelayCallback callback) override {
    network::SCTAuditingReporter::SetRetryDelayForTesting(delay);
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
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    std::move(callback).Run(file.IsValid());
  }

  void CreateSimpleCache(
      mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
          factory,
      const base::FilePath& path,
      CreateSimpleCacheCallback callback) override {
    auto backend_holder = base::MakeRefCounted<
        base::RefCountedData<std::unique_ptr<disk_cache::Backend>>>();
    int rv = disk_cache::CreateCacheBackend(
        net::DISK_CACHE, net::CACHE_BACKEND_SIMPLE,
        base::MakeRefCounted<network::MojoBackendFileOperationsFactory>(
            std::move(factory)),
        path, 64 * 1024 * 1024, disk_cache::ResetHandling::kResetOnError,
        net::NetLog::Get(), &backend_holder->data,
        base::BindOnce(&NetworkServiceTestImpl::OnCacheCreated,
                       weak_factory_.GetWeakPtr(), backend_holder,
                       std::move(callback)));
    DCHECK_EQ(rv, net::ERR_IO_PENDING);
  }

 private:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
    latest_memory_pressure_level_ = memory_pressure_level;
  }

  void OnCacheCreated(
      scoped_refptr<base::RefCountedData<std::unique_ptr<disk_cache::Backend>>>
          backend_holder,
      CreateSimpleCacheCallback callback,
      int rv) {
    DCHECK(backend_holder);
    std::unique_ptr<disk_cache::Backend> backend =
        std::move(backend_holder->data);
    if (rv != net::OK) {
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

  bool registered_as_destruction_observer_ = false;
  bool have_test_doh_servers_ = false;
  mojo::ReceiverSet<network::mojom::NetworkServiceTest> receivers_;
  std::unique_ptr<TestHostResolver> test_host_resolver_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;
  std::unique_ptr<net::ScopedTransportSecurityStateSource>
      transport_security_state_source_;
  base::MemoryPressureListener memory_pressure_listener_;
  base::MemoryPressureListener::MemoryPressureLevel
      latest_memory_pressure_level_ =
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  std::unique_ptr<disk_cache::Backend> disk_cache_backend_;

  base::WeakPtrFactory<NetworkServiceTestImpl> weak_factory_{this};
};

NetworkServiceTestHelper::NetworkServiceTestHelper()
    : network_service_test_impl_(new NetworkServiceTestImpl) {}

NetworkServiceTestHelper::~NetworkServiceTestHelper() = default;

void NetworkServiceTestHelper::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface(base::BindRepeating(
      &NetworkServiceTestHelper::BindNetworkServiceTestReceiver,
      base::Unretained(this)));
}

void NetworkServiceTestHelper::BindNetworkServiceTestReceiver(
    mojo::PendingReceiver<network::mojom::NetworkServiceTest> receiver) {
  network_service_test_impl_->BindReceiver(std::move(receiver));
}

}  // namespace content
