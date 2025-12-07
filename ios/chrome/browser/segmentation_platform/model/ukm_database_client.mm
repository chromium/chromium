// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/ukm_database_client.h"

#import <utility>

#import "base/check_is_test.h"
#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "base/path_service.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/segmentation_platform/internal/dummy_ukm_data_manager.h"
#import "components/segmentation_platform/internal/signals/ukm_observer.h"
#import "components/segmentation_platform/internal/ukm_data_manager_impl.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/local_state_helper.h"
#import "components/ukm/ukm_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"

namespace segmentation_platform {

UkmDatabaseClient::UkmDatabaseClient() {
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformUkmEngine)) {
    ukm_data_manager_ = std::make_unique<UkmDataManagerImpl>();
  } else {
    ukm_data_manager_ = std::make_unique<DummyUkmDataManager>();
  }
}

UkmDatabaseClient::~UkmDatabaseClient() = default;

UkmDataManager* UkmDatabaseClient::GetUkmDataManager() {
  CHECK(ukm_data_manager_);
  return ukm_data_manager_.get();
}

void UkmDatabaseClient::PreProfileInit(bool in_memory_database) {
  segmentation_platform::LocalStateHelper::GetInstance().Initialize(
      GetApplicationContext()->GetLocalState());

  // Path service is setup at early startup.
  base::FilePath local_data_dir;
  bool result = base::PathService::Get(ios::DIR_USER_DATA, &local_data_dir);
  DCHECK(result);
  ukm_data_manager_->Initialize(
      local_data_dir.Append(FILE_PATH_LITERAL("segmentation_platform/ukm_db")),
      in_memory_database);
}

void UkmDatabaseClient::TearDownForTesting() {
  ukm_data_manager_.reset();
  ukm_observer_.reset();
  ukm_recorder_for_testing_ = nullptr;
}

void UkmDatabaseClient::StartObservation() {
  CHECK(!ukm_observer_);
  if (ukm_recorder_for_testing_) {
    CHECK_IS_TEST();
    ukm_observer_ = std::make_unique<UkmObserver>(ukm_recorder_for_testing_);
  } else {
    auto* ukm_service =
        GetApplicationContext()->GetMetricsServicesManager()->GetUkmService();
    ukm_observer_ = std::make_unique<UkmObserver>(ukm_service);
    // First UKM state notification at startup is sent when UKM service is
    // created by IOSChromeMetricsServiceClient::Initialize(). So, update the
    // observer with the current consent state.
    ukm_observer_->InitalizeUkmAllowedState(
        ukm_service->recording_enabled(ukm::MSBB));
  }
  ukm_data_manager_->StartObservation(ukm_observer_.get());
}

void UkmDatabaseClient::PostMessageLoopRun() {
  // UkmService is destroyed in ApplicationContextImpl::StartTearDown(), which
  // happens after all the extra main parts get PostMainMessageLoopRun(). So, it
  // is safe to stop the observer here. The profiles can still be active and
  // UkmDataManager needs to be available. This does not tear down the
  // UkmDataManager, but only stops observing UKM.
  if (ukm_observer_) {
    // Some of the content browser implementations do not invoke
    // PreProfileInit().
    ukm_observer_->StopObserving();
    ukm_observer_ = nullptr;
  }
}

// static
UkmDatabaseClientHolder& UkmDatabaseClientHolder::GetInstance() {
  static base::NoDestructor<UkmDatabaseClientHolder> instance;
  return *instance;
}

// static
UkmDatabaseClient& UkmDatabaseClientHolder::GetClientInstance(
    ProfileIOS* profile) {
  UkmDatabaseClientHolder& instance = GetInstance();
  base::AutoLock l(instance.lock_);
  if (!instance.clients_for_testing_.empty()) {
    CHECK_IS_TEST();
    CHECK(profile);
    CHECK(instance.clients_for_testing_.count(profile));
    return *instance.clients_for_testing_[profile];
  }
  return *instance.main_client_;
}

// static
void UkmDatabaseClientHolder::SetUkmClientForTesting(
    ProfileIOS* profile,
    UkmDatabaseClient* client) {
  UkmDatabaseClientHolder& instance = GetInstance();
  instance.SetUkmClientForTestingInternal(profile, client);
}

UkmDatabaseClientHolder::UkmDatabaseClientHolder()
    : main_client_(std::make_unique<UkmDatabaseClient>()) {}

UkmDatabaseClientHolder::~UkmDatabaseClientHolder() = default;

void UkmDatabaseClientHolder::SetUkmClientForTestingInternal(
    ProfileIOS* profile,
    UkmDatabaseClient* client) {
  base::AutoLock l(lock_);
  CHECK(profile);
  if (client) {
    clients_for_testing_[profile] = client;
  } else {
    clients_for_testing_.erase(profile);
  }
}

}  // namespace segmentation_platform
