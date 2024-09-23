// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_sdk_delegate_ios.h"

#import "base/functional/callback.h"
#import "base/notimplemented.h"

namespace data_sharing {

DataSharingSDKDelegateIOS::DataSharingSDKDelegateIOS() = default;
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
  NOTIMPLEMENTED();
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
