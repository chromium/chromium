// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/tracker_factory_util.h"

#import "base/memory/ptr_util.h"
#import "base/memory/ref_counted.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  return base::WrapUnique(feature_engagement::Tracker::Create(
      storage_dir, background_task_runner, db_provider));
}

}  // namespace feature_engagement
