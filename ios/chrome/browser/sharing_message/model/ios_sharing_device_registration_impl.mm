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
#import "components/sharing_message/vapid_key_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_device_info/device_info.h"
#import "crypto/ec_private_key.h"

using instance_id::InstanceID;
using sync_pb::SharingSpecificFields;

IOSSharingDeviceRegistrationImpl::IOSSharingDeviceRegistrationImpl(
    PrefService* pref_service,
    SharingSyncPreference* sharing_sync_preference,
    VapidKeyManager* vapid_key_manager,
    instance_id::InstanceIDDriver* instance_id_driver,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sharing_sync_preference_(sharing_sync_preference),
      vapid_key_manager_(vapid_key_manager),
      instance_id_driver_(instance_id_driver),
      sync_service_(sync_service) {}

IOSSharingDeviceRegistrationImpl::~IOSSharingDeviceRegistrationImpl() = default;

void IOSSharingDeviceRegistrationImpl::RegisterDevice(
    RegistrationCallback callback) {
  std::optional<std::string> authorized_entity = GetAuthorizationEntity();
  if (!authorized_entity) {
    OnVapidTargetInfoRetrieved(std::move(callback),
                               /*authorized_entity=*/std::nullopt,
                               SharingDeviceRegistrationResult::kSuccess,
                               /*vapid_target_info=*/std::nullopt);
    return;
  }

  RetrieveTargetInfo(
      *authorized_entity,
      base::BindOnce(
          &IOSSharingDeviceRegistrationImpl::OnVapidTargetInfoRetrieved,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          *authorized_entity));
}

void IOSSharingDeviceRegistrationImpl::RetrieveTargetInfo(
    const std::string& authorized_entity,
    TargetInfoCallback callback) {
  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->GetToken(
          authorized_entity, instance_id::kGCMScope,
          /*time_to_live=*/base::TimeDelta(),
          /*flags=*/{InstanceID::Flags::kBypassScheduler},
          base::BindOnce(&IOSSharingDeviceRegistrationImpl::OnFCMTokenReceived,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         authorized_entity));
}

void IOSSharingDeviceRegistrationImpl::OnFCMTokenReceived(
    TargetInfoCallback callback,
    const std::string& authorized_entity,
    const std::string& fcm_token,
    instance_id::InstanceID::Result result) {
  switch (result) {
    case InstanceID::SUCCESS:
      instance_id_driver_->GetInstanceID(kSharingFCMAppID)
          ->GetEncryptionInfo(
              authorized_entity,
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

void IOSSharingDeviceRegistrationImpl::OnVapidTargetInfoRetrieved(
    RegistrationCallback callback,
    std::optional<std::string> authorized_entity,
    SharingDeviceRegistrationResult result,
    std::optional<syncer::DeviceInfo::SharingTargetInfo> vapid_target_info) {
  if (result != SharingDeviceRegistrationResult::kSuccess) {
    std::move(callback).Run(result);
    return;
  }

  if (!CanSendViaSenderID(sync_service_)) {
    OnSharingTargetInfoRetrieved(
        std::move(callback), std::move(authorized_entity),
        std::move(vapid_target_info), SharingDeviceRegistrationResult::kSuccess,
        /*sharing_target_info=*/std::nullopt);
    return;
  }

  // Attempt to register using sender ID when enabled.
  RetrieveTargetInfo(
      kSharingSenderID,
      base::BindOnce(
          &IOSSharingDeviceRegistrationImpl::OnSharingTargetInfoRetrieved,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          std::move(authorized_entity), std::move(vapid_target_info)));
}

void IOSSharingDeviceRegistrationImpl::OnSharingTargetInfoRetrieved(
    RegistrationCallback callback,
    std::optional<std::string> authorized_entity,
    std::optional<syncer::DeviceInfo::SharingTargetInfo> vapid_target_info,
    SharingDeviceRegistrationResult result,
    std::optional<syncer::DeviceInfo::SharingTargetInfo> sharing_target_info) {
  if (result != SharingDeviceRegistrationResult::kSuccess) {
    std::move(callback).Run(result);
    return;
  }

  if (!vapid_target_info && !sharing_target_info) {
    std::move(callback).Run(SharingDeviceRegistrationResult::kInternalError);
    return;
  }

  base::UmaHistogramBoolean("Sharing.LocalSharingTargetInfoSupportsSync",
                            !!sharing_target_info);
  std::set<SharingSpecificFields::EnabledFeatures> enabled_features =
      GetEnabledFeatures(/*supports_vapid=*/authorized_entity.has_value());
  syncer::DeviceInfo::SharingInfo sharing_info(
      vapid_target_info ? std::move(*vapid_target_info)
                        : syncer::DeviceInfo::SharingTargetInfo(),
      sharing_target_info ? std::move(*sharing_target_info)
                          : syncer::DeviceInfo::SharingTargetInfo(),
      /*chime_representative_target_id=*/std::string(),
      std::move(enabled_features));
  sharing_sync_preference_->SetLocalSharingInfo(std::move(sharing_info));
  sharing_sync_preference_->SetFCMRegistration(
      // Clears authorized_entity in preferences if it's not populated.
      SharingSyncPreference::FCMRegistration(std::move(authorized_entity),
                                             base::Time::Now()));
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

  if (!registration->authorized_entity) {
    OnVapidFCMTokenDeleted(std::move(callback),
                           SharingDeviceRegistrationResult::kSuccess);
    return;
  }

  DeleteFCMToken(
      *registration->authorized_entity,
      base::BindOnce(&IOSSharingDeviceRegistrationImpl::OnVapidFCMTokenDeleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IOSSharingDeviceRegistrationImpl::OnVapidFCMTokenDeleted(
    RegistrationCallback callback,
    SharingDeviceRegistrationResult result) {
  if (result != SharingDeviceRegistrationResult::kSuccess) {
    std::move(callback).Run(result);
    return;
  }

  DeleteFCMToken(kSharingSenderID, std::move(callback));
}

void IOSSharingDeviceRegistrationImpl::DeleteFCMToken(
    const std::string& authorized_entity,
    RegistrationCallback callback) {
  instance_id_driver_->GetInstanceID(kSharingFCMAppID)
      ->DeleteToken(
          authorized_entity, instance_id::kGCMScope,
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

  NOTREACHED_IN_MIGRATION();
}

std::optional<std::string>
IOSSharingDeviceRegistrationImpl::GetAuthorizationEntity() const {
  crypto::ECPrivateKey* vapid_key = vapid_key_manager_->GetOrCreateKey();
  if (!vapid_key) {
    return std::nullopt;
  }

  std::string public_key;
  if (!gcm::GetRawPublicKey(*vapid_key, &public_key)) {
    return std::nullopt;
  }

  std::string base64_public_key;
  base::Base64UrlEncode(public_key, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_public_key);
  return std::make_optional(std::move(base64_public_key));
}

std::set<SharingSpecificFields::EnabledFeatures>
IOSSharingDeviceRegistrationImpl::GetEnabledFeatures(
    bool supports_vapid) const {
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
