// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "components/sharing_message/sharing_message_bridge_impl.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/channel_info.h"

IOSSharingMessageBridgeFactory::IOSSharingMessageBridgeFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "SharingMessageBridge") {}

std::unique_ptr<KeyedService>
IOSSharingMessageBridgeFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::SHARING_MESSAGE,
          base::BindRepeating(&syncer::ReportUnrecoverableError, GetChannel()));
  return std::make_unique<SharingMessageBridgeImpl>(
      std::move(change_processor));
}
