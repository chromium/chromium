// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"

#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/power_bookmarks/core/power_bookmark_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
power_bookmarks::PowerBookmarkService*
PowerBookmarkServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<power_bookmarks::PowerBookmarkService>(
          profile, /*create=*/true);
}

// static
PowerBookmarkServiceFactory* PowerBookmarkServiceFactory::GetInstance() {
  static base::NoDestructor<PowerBookmarkServiceFactory> instance;
  return instance.get();
}

PowerBookmarkServiceFactory::PowerBookmarkServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PowerBookmarkService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
}

PowerBookmarkServiceFactory::~PowerBookmarkServiceFactory() = default;

std::unique_ptr<KeyedService>
PowerBookmarkServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(state);

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForProfile(profile);

  return std::make_unique<power_bookmarks::PowerBookmarkService>(
      bookmark_model, state->GetStatePath().AppendASCII("power_bookmarks"),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
}
