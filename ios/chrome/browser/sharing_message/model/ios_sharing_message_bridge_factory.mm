// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"

#import "base/feature_list.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/send_tab_to_self/features.h"
#import "components/sharing_message/sharing_message_bridge_impl.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/channel_info.h"

// static
SharingMessageBridge* IOSSharingMessageBridgeFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<SharingMessageBridge*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SharingMessageBridge* IOSSharingMessageBridgeFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<SharingMessageBridge*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
IOSSharingMessageBridgeFactory* IOSSharingMessageBridgeFactory::GetInstance() {
  static base::NoDestructor<IOSSharingMessageBridgeFactory> instance;
  return instance.get();
}

IOSSharingMessageBridgeFactory::IOSSharingMessageBridgeFactory()
    : BrowserStateKeyedServiceFactory(
          "SharingMessageBridge",
          BrowserStateDependencyManager::GetInstance()) {}

IOSSharingMessageBridgeFactory::~IOSSharingMessageBridgeFactory() {}

std::unique_ptr<KeyedService>
IOSSharingMessageBridgeFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    return nullptr;
  }

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::SHARING_MESSAGE,
          base::BindRepeating(&syncer::ReportUnrecoverableError, GetChannel()));
  return std::make_unique<SharingMessageBridgeImpl>(
      std::move(change_processor));
}
