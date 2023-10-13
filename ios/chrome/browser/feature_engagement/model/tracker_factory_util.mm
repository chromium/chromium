// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/tracker_factory_util.h"

#import "base/memory/ptr_util.h"
#import "base/memory/ref_counted.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter.h"
#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace {

// Directory in which FeatureEngagementTracker data can be stored.
const base::FilePath::CharType kIOSFeatureEngagementTrackerStorageDirname[] =
    FILE_PATH_LITERAL("Feature Engagement Tracker");

}  // namespace

namespace feature_engagement {

std::unique_ptr<KeyedService> CreateFeatureEngagementTracker(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  base::FilePath storage_dir = browser_state->GetStatePath().Append(
      kIOSFeatureEngagementTrackerStorageDirname);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      browser_state->GetProtoDatabaseProvider();

  base::WeakPtr<PromosManagerEventExporter> event_exporter =
      ShouldPromosManagerUseFET()
          ? PromosManagerEventExporterFactory::GetForBrowserState(browser_state)
                ->AsWeakPtr()
          : nullptr;

  return base::WrapUnique(feature_engagement::Tracker::Create(
      storage_dir, background_task_runner, db_provider, event_exporter));
}

}  // namespace feature_engagement
