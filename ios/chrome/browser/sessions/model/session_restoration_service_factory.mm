// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"

#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Value taken from Desktop Chrome.
constexpr base::TimeDelta kSaveDelay = base::Seconds(2.5);

}  // namespace

// static
SessionRestorationService* SessionRestorationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SessionRestorationService>(
      profile, /*create=*/true);
}

// static
SessionRestorationServiceFactory*
SessionRestorationServiceFactory::GetInstance() {
  static base::NoDestructor<SessionRestorationServiceFactory> instance;
  return instance.get();
}

SessionRestorationServiceFactory::SessionRestorationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SessionRestorationService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

SessionRestorationServiceFactory::~SessionRestorationServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionRestorationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<SessionRestorationServiceImpl>(
      kSaveDelay, IsPinnedTabsEnabled(), profile->GetStatePath(),
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
}
