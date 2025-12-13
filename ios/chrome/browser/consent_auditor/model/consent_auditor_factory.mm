// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/consent_auditor/model/consent_auditor_factory.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/consent_auditor/consent_auditor_impl.h"
#import "components/consent_auditor/consent_sync_bridge.h"
#import "components/consent_auditor/consent_sync_bridge_impl.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"

// static
consent_auditor::ConsentAuditor* ConsentAuditorFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<consent_auditor::ConsentAuditor>(
      profile, /*create=*/true);
}

// static
ConsentAuditorFactory* ConsentAuditorFactory::GetInstance() {
  static base::NoDestructor<ConsentAuditorFactory> instance;
  return instance.get();
}

ConsentAuditorFactory::ConsentAuditorFactory()
    : ProfileKeyedServiceFactoryIOS("ConsentAuditor") {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

ConsentAuditorFactory::~ConsentAuditorFactory() {}

std::unique_ptr<KeyedService> ConsentAuditorFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  std::unique_ptr<consent_auditor::ConsentSyncBridge> consent_sync_bridge;
  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::USER_CONSENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              ::GetChannel()));
  consent_sync_bridge =
      std::make_unique<consent_auditor::ConsentSyncBridgeImpl>(
          std::move(store_factory), std::move(change_processor));

  // The locale doesn't change at runtime, so we can pass it directly.
  return std::make_unique<consent_auditor::ConsentAuditorImpl>(
      std::move(consent_sync_bridge),
      GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
      base::DefaultClock::GetInstance());
}
