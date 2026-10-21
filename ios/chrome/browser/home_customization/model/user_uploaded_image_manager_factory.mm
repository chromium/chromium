// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager_factory.h"

#import <utility>

#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

UserUploadedImageManagerFactory::UserUploadedImageManagerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "UserUploadedImageManager") {}

std::unique_ptr<KeyedService>
UserUploadedImageManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<UserUploadedImageManager>(
      profile->GetStatePath(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
}
