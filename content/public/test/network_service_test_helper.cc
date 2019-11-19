// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/network_service_test_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/field_trial.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_host_resolver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/cookie_manager.h"
#include "services/network/host_resolver.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if defined(OS_ANDROID)
#include "base/test/android/url_utils.h"
#include "base/test/test_support_android.h"
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
}  // namespace

class NetworkServiceTestHelper::NetworkServiceTestImpl
    : public network::mojom::NetworkServiceTest,
      public base::MessageLoopCurrent::DestructionObserver {
 public:
  NetworkServiceTestImpl()
      : memory_pressure_listener_(
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

  ~NetworkServiceTestImpl() override {
    network::NetworkContext::SetCertVerifierForTesting(nullptr);
  }

  // network::mojom::NetworkServiceTest:
  void AddRules(std::vector<network::mojom::RulePtr> rules,
                AddRulesCallback callback) override {
    auto* host_resolver = test_host_resolver_.host_resolver();
    for (const auto& rule : rules) {
      switch (rule->resolver_type) {
        case network::mojom::ResolverType::kResolverTypeFail:
          host_resolver->AddSimulatedFailure(rule->host_pattern);
          break;
        case network::mojom::ResolverType::kResolverTypeIPLiteral:
          host_resolver->AddIPLiteralRule(rule->host_pattern, rule->replacement,
                                          std::string());
          break;
        case network::mojom::ResolverType::kResolverTypeDirectLookup:
          host_resolver->AllowDirectLookup(rule->host_pattern);
          break;
        default:
          host_resolver->AddRule(rule->host_pattern, rule->replacement);
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

  void SetShouldRequireCT(ShouldRequireCT required,
                          SetShouldRequireCTCallback callback) override {
    if (required == NetworkServiceTest::ShouldRequireCT::RESET) {
      net::TransportSecurityState::SetShouldRequireCTForTesting(nullptr);
      std::move(callback).Run();
      return;
    }

    bool ct = true;
    if (NetworkServiceTest::ShouldRequireCT::DONT_REQUIRE == required)
      ct = false;

    net::TransportSecurityState::SetShouldRequireCTForTesting(&ct);
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
      base::MessageLoopCurrentForIO::Get()->AddDestructionObserver(this);
      registered_as_destruction_observer_ = true;
    }
  }

  // base::MessageLoopCurrent::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    // Needs to be called on the IO thread.
    receivers_.Clear();
  }

 private:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
    latest_memory_pressure_level_ = memory_pressure_level;
  }

  bool registered_as_destruction_observer_ = false;
  mojo::ReceiverSet<network::mojom::NetworkServiceTest> receivers_;
  TestHostResolver test_host_resolver_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;
  std::unique_ptr<net::ScopedTransportSecurityStateSource>
      transport_security_state_source_;
  base::MemoryPressureListener memory_pressure_listener_;
  base::MemoryPressureListener::MemoryPressureLevel
      latest_memory_pressure_level_ =
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestImpl);
};

NetworkServiceTestHelper::NetworkServiceTestHelper()
    : network_service_test_impl_(new NetworkServiceTestImpl) {}

NetworkServiceTestHelper::~NetworkServiceTestHelper() = default;

void NetworkServiceTestHelper::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface(
      base::Bind(&NetworkServiceTestHelper::BindNetworkServiceTestReceiver,
                 base::Unretained(this)));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  service_manager::SandboxType sandbox_type =
      service_manager::SandboxTypeFromCommandLine(*command_line);
  if (IsUnsandboxedSandboxType(sandbox_type) ||
      sandbox_type == service_manager::SANDBOX_TYPE_NETWORK) {
    // Register the EmbeddedTestServer's certs, so that any SSL connections to
    // it succeed. Only do this when file I/O is allowed in the current process.
#if defined(OS_ANDROID)
    base::InitAndroidTestPaths(base::android::GetIsolatedTestRoot());
#endif

    if (!command_line->HasSwitch(switches::kDisableTestCerts)) {
      net::EmbeddedTestServer::RegisterTestCerts();
      net::SpawnedTestServer::RegisterTestCerts();

      // Also add the QUIC test certificate.
      net::TestRootCerts* root_certs = net::TestRootCerts::GetInstance();
      root_certs->AddFromFile(
          net::GetTestCertsDirectory().AppendASCII("quic-root.pem"));
    }
  }
}

void NetworkServiceTestHelper::BindNetworkServiceTestReceiver(
    mojo::PendingReceiver<network::mojom::NetworkServiceTest> receiver) {
  network_service_test_impl_->BindReceiver(std::move(receiver));
}

}  // namespace content
