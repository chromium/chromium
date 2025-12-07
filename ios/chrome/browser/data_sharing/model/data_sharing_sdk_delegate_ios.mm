// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_sdk_delegate_ios.h"

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/notimplemented.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/share_kit/model/share_kit_delete_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_leave_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_lookup_gaia_id_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_read_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

namespace data_sharing {

DataSharingSDKDelegateIOS::DataSharingSDKDelegateIOS(
    ShareKitService* share_kit_service)
    : share_kit_service_(share_kit_service) {}
DataSharingSDKDelegateIOS::~DataSharingSDKDelegateIOS() = default;

void DataSharingSDKDelegateIOS::Initialize(
    DataSharingNetworkLoader* data_sharing_network_loader) {
  // No op.
}

void DataSharingSDKDelegateIOS::CreateGroup(
    const data_sharing_pb::CreateGroupParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::CreateGroupResult,
                                  absl::Status>&)> callback) {
  NOTREACHED();
}

void DataSharingSDKDelegateIOS::ReadGroups(
    const data_sharing_pb::ReadGroupsParams& params,
    base::OnceCallback<void(
        const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>
        callback) {
  NSMutableArray<ShareKitReadGroupParamConfiguration*>* groupsParam =
      [NSMutableArray array];
  for (auto group_param : params.group_params()) {
    ShareKitReadGroupParamConfiguration* param =
        [[ShareKitReadGroupParamConfiguration alloc] init];
    param.collabID = base::SysUTF8ToNSString(group_param.group_id());
    param.consistencyToken =
        base::SysUTF8ToNSString(group_param.consistency_token());
    [groupsParam addObject:param];
  }
  ShareKitReadGroupsConfiguration* config =
      [[ShareKitReadGroupsConfiguration alloc] init];
  config.groupsParam = groupsParam;
  config.callback = base::CallbackToBlock(std::move(callback));
  share_kit_service_->ReadGroups(config);
}

void DataSharingSDKDelegateIOS::ReadGroupWithToken(
    const data_sharing_pb::ReadGroupWithTokenParams& params,
    base::OnceCallback<void(
        const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>
        callback) {
  ShareKitReadGroupWithTokenConfiguration* config =
      [[ShareKitReadGroupWithTokenConfiguration alloc] init];
  config.collabID = base::SysUTF8ToNSString(params.group_id());
  config.tokenSecret = base::SysUTF8ToNSString(params.access_token());
  config.callback = base::CallbackToBlock(std::move(callback));
  share_kit_service_->ReadGroupWithToken(config);
}

void DataSharingSDKDelegateIOS::AddMember(
    const data_sharing_pb::AddMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTREACHED();
}

void DataSharingSDKDelegateIOS::RemoveMember(
    const data_sharing_pb::RemoveMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTREACHED();
}

void DataSharingSDKDelegateIOS::LeaveGroup(
    const data_sharing_pb::LeaveGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  ShareKitLeaveConfiguration* config =
      [[ShareKitLeaveConfiguration alloc] init];
  config.collabID = base::SysUTF8ToNSString(params.group_id());
  config.callback = base::CallbackToBlock(std::move(callback));
  share_kit_service_->LeaveGroup(config);
}

void DataSharingSDKDelegateIOS::DeleteGroup(
    const data_sharing_pb::DeleteGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  ShareKitDeleteConfiguration* config =
      [[ShareKitDeleteConfiguration alloc] init];
  config.collabID = base::SysUTF8ToNSString(params.group_id());
  config.callback = base::CallbackToBlock(std::move(callback));
  share_kit_service_->DeleteGroup(config);
}

void DataSharingSDKDelegateIOS::LookupGaiaIdByEmail(
    const data_sharing_pb::LookupGaiaIdByEmailParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                  absl::Status>&)> callback) {
  ShareKitLookupGaiaIDConfiguration* config =
      [[ShareKitLookupGaiaIDConfiguration alloc] init];
  config.email = base::SysUTF8ToNSString(params.email());
  config.callback = base::CallbackToBlock(std::move(callback));
  share_kit_service_->LookupGaiaIdByEmail(config);
}

void DataSharingSDKDelegateIOS::Shutdown() {
  // No op.
}

void DataSharingSDKDelegateIOS::AddAccessToken(
    const data_sharing_pb::AddAccessTokenParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                  absl::Status>&)> callback) {
  NOTREACHED();
}

}  // namespace data_sharing
