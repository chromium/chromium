// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/tab_context_sync_service_factory.h"

#import <memory>
#import <utility>

#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync_tab_context/tab_context_sync_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
sync_tab_context::TabContextSyncService*
TabContextSyncServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<sync_tab_context::TabContextSyncService>(
          profile, /*create=*/true);
}

// static
TabContextSyncServiceFactory* TabContextSyncServiceFactory::GetInstance() {
  static base::NoDestructor<TabContextSyncServiceFactory> instance;
  return instance.get();
}

TabContextSyncServiceFactory::TabContextSyncServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TabContextSyncService",
                                    TestingCreation::kNoServiceForTests) {}

TabContextSyncServiceFactory::~TabContextSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabContextSyncServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER,
          /*dump_stack=*/base::DoNothing());
  return std::make_unique<sync_tab_context::TabContextSyncServiceImpl>(
      std::move(change_processor));
}
