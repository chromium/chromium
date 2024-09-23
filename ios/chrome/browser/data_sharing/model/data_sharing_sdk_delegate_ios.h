// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SDK_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SDK_DELEGATE_IOS_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace data_sharing {

// Used by DataSharingService to provide access to SDK.
class DataSharingSDKDelegateIOS : public DataSharingSDKDelegate {
 public:
  explicit DataSharingSDKDelegateIOS();
  ~DataSharingSDKDelegateIOS() override;

  DataSharingSDKDelegateIOS(const DataSharingSDKDelegateIOS&) = delete;
  DataSharingSDKDelegateIOS& operator=(const DataSharingSDKDelegateIOS&) =
      delete;
  DataSharingSDKDelegateIOS(DataSharingSDKDelegateIOS&&) = delete;
  DataSharingSDKDelegateIOS& operator=(DataSharingSDKDelegateIOS&&) = delete;

  // DataSharingSDKDelegate:
  void Initialize(
      DataSharingNetworkLoader* data_sharing_network_loader) override;
  void CreateGroup(const data_sharing_pb::CreateGroupParams& params,
                   base::OnceCallback<void(
                       const base::expected<data_sharing_pb::CreateGroupResult,
                                            absl::Status>&)> callback) override;
  void ReadGroups(const data_sharing_pb::ReadGroupsParams& params,
                  base::OnceCallback<void(
                      const base::expected<data_sharing_pb::ReadGroupsResult,
                                           absl::Status>&)> callback) override;
  void AddMember(
      const data_sharing_pb::AddMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;
  void RemoveMember(
      const data_sharing_pb::RemoveMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;
  void DeleteGroup(
      const data_sharing_pb::DeleteGroupParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;
  void LookupGaiaIdByEmail(
      const data_sharing_pb::LookupGaiaIdByEmailParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                    absl::Status>&)> callback) override;
  void Shutdown() override;
  void AddAccessToken(
      const data_sharing_pb::AddAccessTokenParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                    absl::Status>&)> callback) override;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SDK_DELEGATE_IOS_H_
