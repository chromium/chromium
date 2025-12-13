// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager_factory.h"

#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
UserUploadedImageManager* UserUploadedImageManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<UserUploadedImageManager>(
      profile, /*create=*/true);
}

// static
UserUploadedImageManagerFactory*
UserUploadedImageManagerFactory::GetInstance() {
  static base::NoDestructor<UserUploadedImageManagerFactory> instance;
  return instance.get();
}

UserUploadedImageManagerFactory::UserUploadedImageManagerFactory()
    : ProfileKeyedServiceFactoryIOS("UserUploadedImageManager") {}

UserUploadedImageManagerFactory::~UserUploadedImageManagerFactory() {}

std::unique_ptr<KeyedService>
UserUploadedImageManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<UserUploadedImageManager>(
      profile->GetStatePath(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
}
