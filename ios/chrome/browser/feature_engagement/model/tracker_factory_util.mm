// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/tracker_factory_util.h"

#import "base/memory/ptr_util.h"
#import "base/memory/ref_counted.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/default_browser/model/default_browser_promo_event_exporter.h"
#import "ios/chrome/browser/feature_engagement/model/ios_tracker_session_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Directory in which FeatureEngagementTracker data can be stored.
const base::FilePath::CharType kIOSFeatureEngagementTrackerStorageDirname[] =
    FILE_PATH_LITERAL("Feature Engagement Tracker");

}  // namespace

namespace feature_engagement {

std::unique_ptr<KeyedService> CreateFeatureEngagementTracker(
    web::BrowserState* context) {
  std::optional<std::string> fetDemoModeOverride =
      tests_hook::FETDemoModeOverride();
  if (fetDemoModeOverride.has_value()) {
    return CreateDemoModeTracker(fetDemoModeOverride.value());
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  base::FilePath storage_dir = profile->GetStatePath().Append(
      kIOSFeatureEngagementTrackerStorageDirname);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      profile->GetProtoDatabaseProvider();

  auto default_browser_event_exporter =
      std::make_unique<DefaultBrowserEventExporter>();

  auto session_controller = std::make_unique<IOSTrackerSessionController>();

  return base::WrapUnique(feature_engagement::Tracker::Create(
      storage_dir, background_task_runner, db_provider,
      std::move(default_browser_event_exporter),
      feature_engagement::Tracker::GetDefaultConfigurationProviders(),
      std::move(session_controller)));
}

}  // namespace feature_engagement
