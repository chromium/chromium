// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_DEVICE_REGISTRATION_IMPL_H_
#define IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_DEVICE_REGISTRATION_IMPL_H_

#import <optional>
#import <string>

#import "base/functional/callback.h"
#import "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/gcm_driver/instance_id/instance_id.h"
#import "components/sharing_message/sharing_device_registration.h"
#import "components/sync/protocol/device_info_specifics.pb.h"
#import "components/sync_device_info/device_info.h"

class PrefService;

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace syncer {
class SyncService;
}  // namespace syncer

enum class SharingDeviceRegistrationResult;
class SharingSyncPreference;
class VapidKeyManager;

// Responsible for registering and unregistering device with
// SharingSyncPreference.
class IOSSharingDeviceRegistrationImpl : public SharingDeviceRegistration {
 public:
  using RegistrationCallback =
      base::OnceCallback<void(SharingDeviceRegistrationResult)>;
  using TargetInfoCallback = base::OnceCallback<void(
      SharingDeviceRegistrationResult,
      std::optional<syncer::DeviceInfo::SharingTargetInfo>)>;

  IOSSharingDeviceRegistrationImpl(
      PrefService* pref_service,
      SharingSyncPreference* prefs,
      VapidKeyManager* vapid_key_manager,
      instance_id::InstanceIDDriver* instance_id_driver,
      syncer::SyncService* sync_service);

  IOSSharingDeviceRegistrationImpl(const IOSSharingDeviceRegistrationImpl&) =
      delete;
  IOSSharingDeviceRegistrationImpl& operator=(
      const IOSSharingDeviceRegistrationImpl&) = delete;

  ~IOSSharingDeviceRegistrationImpl() override;

  // SharingDeviceRegistration:
  void RegisterDevice(RegistrationCallback callback) override;
  void UnregisterDevice(RegistrationCallback callback) override;
  bool IsClickToCallSupported() const override;
  bool IsSharedClipboardSupported() const override;
  bool IsSmsFetcherSupported() const override;
  bool IsRemoteCopySupported() const override;
  bool IsOptimizationGuidePushNotificationSupported() const override;
  void SetEnabledFeaturesForTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features)
      override;

 private:
  FRIEND_TEST_ALL_PREFIXES(IOSSharingDeviceRegistrationImplTest,
                           RegisterDeviceTest_Success);

  void RetrieveTargetInfo(const std::string& authorized_entity,
                          TargetInfoCallback callback);

  void OnFCMTokenReceived(TargetInfoCallback callback,
                          const std::string& authorized_entity,
                          const std::string& fcm_token,
                          instance_id::InstanceID::Result result);

  void OnEncryptionInfoReceived(TargetInfoCallback callback,
                                const std::string& fcm_token,
                                std::string p256dh,
                                std::string auth_secret);

  void OnVapidTargetInfoRetrieved(
      RegistrationCallback callback,
      std::optional<std::string> authorized_entity,
      SharingDeviceRegistrationResult result,
      std::optional<syncer::DeviceInfo::SharingTargetInfo> vapid_target_info);

  void OnSharingTargetInfoRetrieved(
      RegistrationCallback callback,
      std::optional<std::string> authorized_entity,
      std::optional<syncer::DeviceInfo::SharingTargetInfo> vapid_target_info,
      SharingDeviceRegistrationResult result,
      std::optional<syncer::DeviceInfo::SharingTargetInfo> sharing_target_info);

  void OnVapidFCMTokenDeleted(RegistrationCallback callback,
                              SharingDeviceRegistrationResult result);

  void DeleteFCMToken(const std::string& authorized_entity,
                      RegistrationCallback callback);

  void OnFCMTokenDeleted(RegistrationCallback callback,
                         instance_id::InstanceID::Result result);

  // Returns the authorization entity for FCM registration.
  std::optional<std::string> GetAuthorizationEntity() const;

  // Computes and returns a set of all enabled features on the device.
  // `supports_vapid`: If set to true, then enabled features with VAPID suffix
  // will be returned, meaning old clients can send VAPID message to this device
  // for those features.
  std::set<sync_pb::SharingSpecificFields_EnabledFeatures> GetEnabledFeatures(
      bool supports_vapid) const;

  raw_ptr<PrefService> pref_service_;
  raw_ptr<SharingSyncPreference> sharing_sync_preference_;
  raw_ptr<VapidKeyManager> vapid_key_manager_;
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  raw_ptr<syncer::SyncService> sync_service_;
  std::optional<std::set<sync_pb::SharingSpecificFields_EnabledFeatures>>
      enabled_features_testing_value_;

  base::WeakPtrFactory<IOSSharingDeviceRegistrationImpl> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_DEVICE_REGISTRATION_IMPL_H_
