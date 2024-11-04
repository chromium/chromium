// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_sdk_delegate_ios.h"

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/notimplemented.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/share_kit/model/share_kit_read_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

namespace data_sharing {

DataSharingSDKDelegateIOS::DataSharingSDKDelegateIOS(
    ShareKitService* share_kit_service)
    : share_kit_service_(share_kit_service) {}
DataSharingSDKDelegateIOS::~DataSharingSDKDelegateIOS() = default;

void DataSharingSDKDelegateIOS::Initialize(
    DataSharingNetworkLoader* data_sharing_network_loader) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::CreateGroup(
    const data_sharing_pb::CreateGroupParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::CreateGroupResult,
                                  absl::Status>&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::ReadGroups(
    const data_sharing_pb::ReadGroupsParams& params,
    base::OnceCallback<void(
        const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>
        callback) {
  NSMutableArray<NSString*>* ids = [NSMutableArray array];
  for (auto group_id : params.group_ids()) {
    NSString* collab_id = base::SysUTF8ToNSString(group_id);
    [ids addObject:collab_id];
  }
  ShareKitReadConfiguration* config = [[ShareKitReadConfiguration alloc] init];
  config.collabIDs = ids;
  config.callback = base::CallbackToBlock(std::move(callback));
  share_kit_service_->ReadGroups(config);
}

void DataSharingSDKDelegateIOS::AddMember(
    const data_sharing_pb::AddMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::RemoveMember(
    const data_sharing_pb::RemoveMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::LeaveGroup(
    const data_sharing_pb::LeaveGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::DeleteGroup(
    const data_sharing_pb::DeleteGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::LookupGaiaIdByEmail(
    const data_sharing_pb::LookupGaiaIdByEmailParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                  absl::Status>&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::Shutdown() {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateIOS::AddAccessToken(
    const data_sharing_pb::AddAccessTokenParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                  absl::Status>&)> callback) {
  NOTIMPLEMENTED();
}

}  // namespace data_sharing
