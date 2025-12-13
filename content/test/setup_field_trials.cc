// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/test/setup_field_trials.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "cc/base/switches.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif

namespace content {
namespace {

// TODO(crbug.com/40772375): Consider not needing VariationsServiceClient just
// to use VariationsFieldTrialCreator.
class VariationServiceClient : public variations::VariationsServiceClient {
 public:
  explicit VariationServiceClient(base::FilePath user_data_dir)
      : user_data_dir_(std::move(user_data_dir)) {}
  ~VariationServiceClient() override = default;
  VariationServiceClient(const VariationServiceClient&) = delete;
  VariationServiceClient(VariationServiceClient&&) = delete;
  VariationServiceClient& operator=(const VariationServiceClient&) = delete;
  VariationServiceClient& operator=(VariationServiceClient&&) = delete;

  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() final { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() final {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() final {
    return nullptr;
  }
  version_info::Channel GetChannel() final {
    return version_info::Channel::UNKNOWN;
  }
  bool OverridesRestrictParameter(std::string*) final { return false; }
  base::FilePath GetVariationsSeedFileDir() final { return user_data_dir_; }
  bool IsEnterprise() final { return false; }
  // Profiles aren't supported, so nothing to do here.
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(PrefService*) final {}

 private:
  base::FilePath user_data_dir_;
};

}  // namespace

// Note that several objects, created here, are scoped to the lifetime of this
// function. They are only needed to set up field trials. It replaces the
// global base::FeatureList instance, which is the only important part.
void SetupFieldTrials() {
  // The environment requires a ThreadPool to post tasks. Create one if
  // needed. It will be torn down at the end of this function, so that it
  // doesn't interfere with tests that need their own ThreadPool.
  const bool need_thread_pool = !base::ThreadPoolInstance::Get();
  if (need_thread_pool) {
    base::ThreadPoolInstance::Create("SetupFieldTrials");
  }

  base::ScopedTempDir user_data_dir;
  CHECK(user_data_dir.CreateUniqueTempDir());
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

  PrefServiceFactory factory;
  factory.set_user_prefs(base::MakeRefCounted<InMemoryPrefStore>());
  std::unique_ptr<PrefService> pref_service = factory.Create(pref_registry);
  CHECK(pref_service);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::vector<base::FeatureList::FeatureOverrideInfo> feature_overrides =
      GetSwitchDependentFeatureOverrides(*command_line);

  // Needed so that content_shell can use fieldtrial_testing_config.
  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/false,
                                                           /*enabled=*/false);

  const bool force_benchmarking_mode =
      command_line->HasSwitch(switches::kEnableGpuBenchmarking);
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          pref_service.get(), &enabled_state_provider, std::wstring(),
          user_data_dir.GetPath(), metrics::StartupVisibility::kUnknown,
          {
              .force_benchmarking_mode = force_benchmarking_mode,
          });
  CHECK(metrics_state_manager);
  metrics_state_manager->InstantiateFieldTrialList();

  std::unique_ptr<variations::SeedResponse> initial_seed;
#if BUILDFLAG(IS_ANDROID)
  if (!pref_service.get()->HasPrefPath(
          variations::prefs::kVariationsSeedSignature)) {
    DVLOG(1) << "Importing first run seed from Java preferences.";
    initial_seed = variations::android::GetVariationsFirstRunSeed();
  }
#endif

  VariationServiceClient variations_service_client(user_data_dir.GetPath());
  variations::VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client,
      std::make_unique<variations::VariationsSeedStore>(
          pref_service.get(), std::move(initial_seed),
          /*signature_verification_enabled=*/true,
          std::make_unique<variations::VariationsSafeSeedStoreLocalState>(
              pref_service.get(),
              variations_service_client.GetVariationsSeedFileDir(),
              variations_service_client.GetChannelForVariations(),
              /*entropy_providers=*/nullptr),
          variations_service_client.GetChannelForVariations(),
          variations_service_client.GetVariationsSeedFileDir()),
      variations::UIStringOverrider());

  variations::SafeSeedManager safe_seed_manager(pref_service.get());

  const std::vector<std::string> variation_ids;  // Empty for tests.
  const std::string command_line_variation_ids =
      command_line->GetSwitchValueASCII(
          variations::switches::kForceVariationIds);
  auto feature_list = std::make_unique<base::FeatureList>();

  variations::test::ScopedVariationsIdsProvider scoped_ids_provider(
      variations::VariationsIdsProvider::Mode::kUseSignedInState);

  variations::PlatformFieldTrials platform_field_trials;

  // Since this is a test-only code path, some arguments to SetUpFieldTrials are
  // not needed, and thus are set to null or empty values.
  // TODO(crbug.com/40790318): Consider passing a low entropy source.
  field_trial_creator.SetUpFieldTrials(
      variation_ids, command_line_variation_ids, feature_overrides,
      std::move(feature_list), metrics_state_manager.get(),
      &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/false,
      *metrics_state_manager->CreateEntropyProviders(
          /*enable_limited_entropy_mode=*/false));

  // Tear down the temporary ThreadPool, so that it doesn't interfere with
  // tests that needs their own ThreadPool.
  if (need_thread_pool) {
    // Note that `Start(..)` is needed, because one can't shutdown a thread pool
    // that hasn't been started. `Start(...)` accesses the feature flags, so
    // it can't be called before `SetUpFieldTrials()`.
    base::ThreadPoolInstance::Get()->Start({/*num_threads=*/1});

    base::ThreadPoolInstance::Get()->Shutdown();
    base::ThreadPoolInstance::Get()->JoinForTesting();
    base::ThreadPoolInstance::Set(nullptr);
  }
}

}  // namespace content
