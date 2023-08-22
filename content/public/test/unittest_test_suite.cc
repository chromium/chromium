// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include <memory>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#include "content/app/mojo/mojo_init.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/notification_service_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_host_resolver.h"
#include "content/public/utility/content_utility_client.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/blink.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace content {

class UnitTestTestSuite::UnitTestEventListener
    : public testing::EmptyTestEventListener {
 public:
  UnitTestEventListener(
      base::RepeatingCallback<
          std::unique_ptr<UnitTestTestSuite::ContentClients>()> create_clients,
      base::OnceClosure first_test_start_callback)
      : create_clients_(create_clients),
        first_test_start_callback_(std::move(first_test_start_callback)) {}

  UnitTestEventListener(const UnitTestEventListener&) = delete;
  UnitTestEventListener& operator=(const UnitTestEventListener&) = delete;

  void InitializeObjects() {
    test_network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
    SetNetworkConnectionTrackerForTesting(
        network::TestNetworkConnectionTracker::GetInstance());

    notification_service_ = std::make_unique<NotificationServiceImpl>();

    content_clients_ = create_clients_.Run();
    CHECK(content_clients_->content_client.get());
    SetContentClient(content_clients_->content_client.get());
    SetBrowserClientForTesting(content_clients_->content_browser_client.get());
    SetUtilityClientForTesting(content_clients_->content_utility_client.get());

    if (first_test_start_callback_)
      std::move(first_test_start_callback_).Run();
  }

  void OnTestStart(const testing::TestInfo& test_info) override {
    InitializeObjects();
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    // Don't call SetUtilityClientForTesting or SetBrowserClientForTesting since
    // if a test overrode ContentClient it might already be deleted and setting
    // these pointers on it would result in a UAF.
    SetContentClient(nullptr);
    content_clients_.reset();

    SetNetworkConnectionTrackerForTesting(nullptr);
    test_network_connection_tracker_.reset();
    notification_service_.reset();

    // If the network::NetworkService object was instantiated during a unit test
    // it will be deleted because network_service_instance.cc has it in a
    // SequenceLocalStorageSlot. However we want to synchronously destruct the
    // InterfacePtr pointing to it to avoid it getting the connection error
    // later and have other tests use the InterfacePtr that is invalid.
    ResetNetworkServiceForTesting();

    breadcrumbs::BreadcrumbManager::GetInstance().ResetForTesting();
  }

 private:
  base::RepeatingCallback<std::unique_ptr<UnitTestTestSuite::ContentClients>()>
      create_clients_;
  base::OnceClosure first_test_start_callback_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      test_network_connection_tracker_;
  std::unique_ptr<NotificationServiceImpl> notification_service_;
  std::unique_ptr<UnitTestTestSuite::ContentClients> content_clients_;
};

UnitTestTestSuite::ContentClients::ContentClients() = default;
UnitTestTestSuite::ContentClients::~ContentClients() = default;

std::unique_ptr<UnitTestTestSuite::ContentClients>
UnitTestTestSuite::CreateTestContentClients() {
  auto clients = std::make_unique<UnitTestTestSuite::ContentClients>();
  clients->content_client = std::make_unique<TestContentClient>();
  clients->content_browser_client =
      std::make_unique<TestContentBrowserClient>();
  return clients;
}

UnitTestTestSuite::UnitTestTestSuite(
    base::TestSuite* test_suite,
    base::RepeatingCallback<std::unique_ptr<ContentClients>()> create_clients,
    absl::optional<mojo::core::Configuration> child_mojo_config)
    : test_suite_(test_suite), create_clients_(create_clients) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string enabled =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);

  ForceCreateNetworkServiceDirectlyForTesting();
  StoragePartitionImpl::ForceInProcessStorageServiceForTesting();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(CreateTestEventListener());
  listeners.Append(new CheckForLeakedWebUIRegistrations);

  scoped_feature_list_.InitFromCommandLine(enabled, disabled);

  mojo::core::InitFeatures();
  if (command_line->HasSwitch(switches::kTestChildProcess)) {
    // Note that in the main test process, TestBlinkWebUnitTestSupport
    // initializes Mojo; so we only do this in child processes.
    mojo::core::Init(child_mojo_config.value_or(mojo::core::Configuration{}));
  } else {
    mojo::core::Init(mojo::core::Configuration{.is_broker_process = true});
  }

  DCHECK(test_suite);
  test_host_resolver_ = std::make_unique<TestHostResolver>();
  browser_accessibility_state_ = BrowserAccessibilityStateImpl::Create();
}

UnitTestTestSuite::~UnitTestTestSuite() = default;

int UnitTestTestSuite::Run() {
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> aura_env = aura::Env::CreateInstance();
#endif
  std::unique_ptr<url::ScopedSchemeRegistryForTests> scheme_registry;

  // TestEventListeners repeater event propagation is disabled in death test
  // child process so create and set the clients here for it.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "gtest_internal_run_death_test")) {
    // TestSuite::Initialize hasn't run yet, which is what initializes the
    // ResourceBundle. This will be needed by Blink initialization to load
    // resources, so do it temporarily here.
    ContentTestSuiteBase::InitializeResourceBundle();

    CreateTestEventListener()->InitializeObjects();

    // Since Blink initialization ended up using the SchemeRegistry, reset
    // that it was accessed before testSuite::Initialize registers its schemes.
    scheme_registry = std::make_unique<url::ScopedSchemeRegistryForTests>();

    ui::ResourceBundle::CleanupSharedInstance();
  }

  return test_suite_->Run();
}

UnitTestTestSuite::UnitTestEventListener*
UnitTestTestSuite::CreateTestEventListener() {
  return new UnitTestEventListener(
      create_clients_,
      base::BindOnce(&UnitTestTestSuite::OnFirstTestStartComplete,
                     base::Unretained(this)));
}

void UnitTestTestSuite::OnFirstTestStartComplete() {
  // At this point ContentClient and ResourceBundle will be initialized, which
  // this needs.
  blink_test_support_ = std::make_unique<TestBlinkWebUnitTestSupport>();
}

}  // namespace content
