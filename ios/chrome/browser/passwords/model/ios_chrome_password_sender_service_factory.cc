// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/ios_chrome_password_sender_service_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sharing/password_sender_service_impl.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#include "ios/chrome/common/channel_info.h"

// static
IOSChromePasswordSenderServiceFactory*
IOSChromePasswordSenderServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordSenderServiceFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordSenderService*
IOSChromePasswordSenderServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
password_manager::PasswordSenderService*
IOSChromePasswordSenderServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<password_manager::PasswordSenderService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

IOSChromePasswordSenderServiceFactory::IOSChromePasswordSenderServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordSenderService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

IOSChromePasswordSenderServiceFactory::
    ~IOSChromePasswordSenderServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSChromePasswordSenderServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::OUTGOING_PASSWORD_SHARING_INVITATION,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              ::GetChannel()));

  auto sync_bridge = std::make_unique<
      password_manager::OutgoingPasswordSharingInvitationSyncBridge>(
      std::move(change_processor));

  return std::make_unique<password_manager::PasswordSenderServiceImpl>(
      std::move(sync_bridge));
}
