// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_receiver_service_factory.h"

#import <memory>

#import "base/functional/bind.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#import "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"

// static
IOSChromePasswordReceiverServiceFactory*
IOSChromePasswordReceiverServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordReceiverServiceFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordReceiverService*
IOSChromePasswordReceiverServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
password_manager::PasswordReceiverService*
IOSChromePasswordReceiverServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<password_manager::PasswordReceiverService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

IOSChromePasswordReceiverServiceFactory::
    IOSChromePasswordReceiverServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordReceiverService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
}

IOSChromePasswordReceiverServiceFactory::
    ~IOSChromePasswordReceiverServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSChromePasswordReceiverServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  // Since Password Manager doesn't work for non-standard profiles, the
  // PasswordReceiverService also shouldn't be created for such profiles.
  CHECK(!context->IsOffTheRecord());

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::INCOMING_PASSWORD_SHARING_INVITATION,
          base::BindRepeating(&syncer::ReportUnrecoverableError, GetChannel()));
  auto sync_bridge = std::make_unique<
      password_manager::IncomingPasswordSharingInvitationSyncBridge>(
      std::move(change_processor),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());

  return std::make_unique<password_manager::PasswordReceiverServiceImpl>(
      profile->GetPrefs(), std::move(sync_bridge),
      IOSChromeProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      IOSChromeAccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get());
}
