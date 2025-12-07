// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/tracker_factory_util.h"

#import "base/memory/ptr_util.h"
#import "base/memory/ref_counted.h"
#import "base/path_service.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/feature_engagement/public/feature_activation.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/feature_engagement/model/event_exporter.h"
#import "ios/chrome/browser/feature_engagement/model/ios_tracker_session_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Directory in which FeatureEngagementTracker data can be stored.
const base::FilePath::CharType kIOSFeatureEngagementTrackerStorageDirname[] =
    FILE_PATH_LITERAL("Feature Engagement Tracker");

}  // namespace

namespace feature_engagement {

std::unique_ptr<KeyedService> CreateFeatureEngagementTracker(
    ProfileIOS* profile) {
  feature_engagement::FeatureActivation FETDemoModeOverride =
      tests_hook::FETDemoModeOverride();
  switch (FETDemoModeOverride.get_state()) {
    case feature_engagement::FeatureActivation::State::kAllEnabled:
      break;
    case feature_engagement::FeatureActivation::State::kAllDisabled:
    case feature_engagement::FeatureActivation::State::kSingleFeatureEnabled:
      return CreateDemoModeTracker(FETDemoModeOverride);
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  base::FilePath storage_dir = profile->GetStatePath().Append(
      kIOSFeatureEngagementTrackerStorageDirname);

  base::FilePath device_storage_dir;
  base::PathService::Get(ios::DIR_USER_DATA, &device_storage_dir);
  device_storage_dir =
      device_storage_dir.Append(kIOSFeatureEngagementTrackerStorageDirname);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      profile->GetProtoDatabaseProvider();

  auto event_exporter = std::make_unique<EventExporter>();

  auto session_controller = std::make_unique<IOSTrackerSessionController>();

  return feature_engagement::Tracker::Create(
      storage_dir, device_storage_dir, GetApplicationContext()->GetLocalState(),
      background_task_runner, db_provider, std::move(event_exporter),
      feature_engagement::Tracker::GetDefaultConfigurationProviders(),
      std::move(session_controller));
}

}  // namespace feature_engagement
