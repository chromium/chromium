// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_device_registration_impl.h"

#import <stdint.h>

#import <vector>

#import "base/base64url.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "build/build_config.h"
#import "components/gcm_driver/crypto/p256_key_util.h"
#import "components/gcm_driver/instance_id/instance_id_driver.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_service.h"
#import "components/sharing_message/buildflags.h"
#import "components/sharing_message/pref_names.h"
#import "components/sharing_message/sharing_constants.h"
#import "components/sharing_message/sharing_device_registration_result.h"
#import "components/sharing_message/sharing_sync_preference.h"
#import "components/sharing_message/sharing_target_device_info.h"
#import "components/sharing_message/sharing_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_device_info/device_info.h"

using instance_id::InstanceID;
using sync_pb::SharingSpecificFields;

IOSSharingDeviceRegistrationImpl::IOSSharingDeviceRegistrationImpl(
    PrefService* pref_service,
    SharingSyncPreference* sharing_sync_preference,
    instance_id::InstanceIDDriver* instance_id_driver,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sharing_sync_preference_(sharing_sync_preference),
      instance_id_driver_(instance_id_driver),
      sync_service_(sync_service) {}

IOSSharingDeviceRegistrationImpl::~IOSSharingDeviceRegistrationImpl() = default;

void IOSSharingDeviceRegistrationImpl::RegisterDevice(
    RegistrationCallback callback) {
  if (!CanSendViaSenderID(sync_service_)) {
    OnSharingTargetInfoRetrieved(std::move(callback),
                                 SharingDeviceRegistrationResult::kSuccess,
                                 /*sharing_target_info=*/std::nullopt);
    return;
  }

  // Attempt to register using sender ID when enabled.
  RetrieveTargetInfo(
      kSharingSenderID,
      base::BindOnce(
          &IOSSharingDeviceRegistrationImpl::OnSharingTargetInfoRetrieved,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IOSSharingDeviceRegistrationImpl::RetrieveTargetInfo(
    const std::string& sender_id,
    TargetInfoCallback callback) {
  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->GetToken(
          sender_id, instance_id::kGCMScope,
          /*time_to_live=*/base::TimeDelta(),
          /*flags=*/{InstanceID::Flags::kBypassScheduler},
          base::BindOnce(&IOSSharingDeviceRegistrationImpl::OnFCMTokenReceived,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         sender_id));
}

void IOSSharingDeviceRegistrationImpl::OnFCMTokenReceived(
    TargetInfoCallback callback,
    const std::string& sender_id,
    const std::string& fcm_token,
    instance_id::InstanceID::Result result) {
  switch (result) {
    case InstanceID::SUCCESS:
      instance_id_driver_->GetInstanceID(kSharingFCMAppID)
          ->GetEncryptionInfo(
              sender_id,
              base::BindOnce(
                  &IOSSharingDeviceRegistrationImpl::OnEncryptionInfoReceived,
                  weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                  fcm_token));
      break;
    case InstanceID::NETWORK_ERROR:
    case InstanceID::SERVER_ERROR:
    case InstanceID::ASYNC_OPERATION_PENDING:
      std::move(callback).Run(
          SharingDeviceRegistrationResult::kFcmTransientError, std::nullopt);
      break;
    case InstanceID::INVALID_PARAMETER:
    case InstanceID::UNKNOWN_ERROR:
    case InstanceID::DISABLED:
      std::move(callback).Run(SharingDeviceRegistrationResult::kFcmFatalError,
                              std::nullopt);
      break;
  }
}

void IOSSharingDeviceRegistrationImpl::OnEncryptionInfoReceived(
    TargetInfoCallback callback,
    const std::string& fcm_token,
    std::string p256dh,
    std::string auth_secret) {
  std::move(callback).Run(
      SharingDeviceRegistrationResult::kSuccess,
      std::make_optional(syncer::DeviceInfo::SharingTargetInfo{
          fcm_token, p256dh, auth_secret}));
}

void IOSSharingDeviceRegistrationImpl::OnSharingTargetInfoRetrieved(
    RegistrationCallback callback,
    SharingDeviceRegistrationResult result,
    std::optional<syncer::DeviceInfo::SharingTargetInfo> sharing_target_info) {
  if (result != SharingDeviceRegistrationResult::kSuccess) {
    std::move(callback).Run(result);
    return;
  }

  if (!sharing_target_info) {
    std::move(callback).Run(SharingDeviceRegistrationResult::kInternalError);
    return;
  }

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features =
      GetEnabledFeatures();
  syncer::DeviceInfo::SharingInfo sharing_info(
      sharing_target_info ? std::move(*sharing_target_info)
                          : syncer::DeviceInfo::SharingTargetInfo(),
      /*chime_representative_target_id=*/std::string(),
      std::move(enabled_features));
  sharing_sync_preference_->SetLocalSharingInfo(std::move(sharing_info));
  sharing_sync_preference_->SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(base::Time::Now()));
  std::move(callback).Run(SharingDeviceRegistrationResult::kSuccess);
}

void IOSSharingDeviceRegistrationImpl::UnregisterDevice(
    RegistrationCallback callback) {
  auto registration = sharing_sync_preference_->GetFCMRegistration();
  if (!registration) {
    std::move(callback).Run(
        SharingDeviceRegistrationResult::kDeviceNotRegistered);
    return;
  }

  sharing_sync_preference_->ClearLocalSharingInfo();

  DeleteFCMToken(kSharingSenderID, std::move(callback));
}

void IOSSharingDeviceRegistrationImpl::DeleteFCMToken(
    const std::string& sender_id,
    RegistrationCallback callback) {
  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->DeleteToken(
          sender_id, instance_id::kGCMScope,
          base::BindOnce(&IOSSharingDeviceRegistrationImpl::OnFCMTokenDeleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IOSSharingDeviceRegistrationImpl::OnFCMTokenDeleted(
    RegistrationCallback callback,
    InstanceID::Result result) {
  switch (result) {
    case InstanceID::SUCCESS:
      // INVALID_PARAMETER is expected if InstanceID.GetToken hasn't been
      // invoked since restart.
    case InstanceID::INVALID_PARAMETER:
      sharing_sync_preference_->ClearFCMRegistration();
      std::move(callback).Run(SharingDeviceRegistrationResult::kSuccess);
      return;
    case InstanceID::NETWORK_ERROR:
    case InstanceID::SERVER_ERROR:
    case InstanceID::ASYNC_OPERATION_PENDING:
      std::move(callback).Run(
          SharingDeviceRegistrationResult::kFcmTransientError);
      return;
    case InstanceID::UNKNOWN_ERROR:
    case InstanceID::DISABLED:
      std::move(callback).Run(SharingDeviceRegistrationResult::kFcmFatalError);
      return;
  }

  NOTREACHED();
}

std::set<SharingSpecificFields::EnabledFeatures>
IOSSharingDeviceRegistrationImpl::GetEnabledFeatures() const {
  // Used in tests
  if (enabled_features_testing_value_) {
    return enabled_features_testing_value_.value();
  }

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;

  if (IsOptimizationGuidePushNotificationSupported()) {
    enabled_features.insert(
        SharingSpecificFields::OPTIMIZATION_GUIDE_PUSH_NOTIFICATION);
  }

  return enabled_features;
}

bool IOSSharingDeviceRegistrationImpl::IsClickToCallSupported() const {
  return false;
}

bool IOSSharingDeviceRegistrationImpl::IsSharedClipboardSupported() const {
  return false;
}

bool IOSSharingDeviceRegistrationImpl::IsSmsFetcherSupported() const {
  return false;
}

bool IOSSharingDeviceRegistrationImpl::IsRemoteCopySupported() const {
  return false;
}

bool IOSSharingDeviceRegistrationImpl::
    IsOptimizationGuidePushNotificationSupported() const {
  return optimization_guide::features::IsOptimizationHintsEnabled() &&
         optimization_guide::features::IsPushNotificationsEnabled();
}

void IOSSharingDeviceRegistrationImpl::SetEnabledFeaturesForTesting(
    std::set<SharingSpecificFields::EnabledFeatures> enabled_features) {
  enabled_features_testing_value_ = std::move(enabled_features);
}
