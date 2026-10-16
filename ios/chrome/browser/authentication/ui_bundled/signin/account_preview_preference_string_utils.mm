// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/account_preview_preference_string_utils.h"

#import <string>

#import "components/signin/core/browser/account_preview_data_service.h"
#import "components/sync/base/data_type.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Converts sync device form factor to device type string.
std::optional<std::u16string> GetDeviceTypeString(
    sync_pb::SyncEnums_DeviceFormFactor form_factor) {
  switch (form_factor) {
    case sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE:
      return l10n_util::GetStringUTF16(IDS_SIGNIN_DEVICE_TYPE_PHONE);
    case sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET:
      return l10n_util::GetStringUTF16(IDS_SIGNIN_DEVICE_TYPE_TABLET);
    case sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP:
      return l10n_util::GetStringUTF16(IDS_SIGNIN_DEVICE_TYPE_COMPUTER);
    default:
      return std::nullopt;
  }
}

// Struct to contain string ids for a particular signin flow.
struct PromoLabelStringIds {
  int device_bookmarks;
  int all_devices_bookmarks;
  int device_passwords;
  int all_devices_passwords;
  int device_saved_info;
  int all_devices_saved_info;
};

// Chooses an appropriate string id from `PromoLabelStringIds` based on
// `preference`.
NSString* GetPromoLabelStringWithIds(
    const std::optional<
        signin::AccountPreviewDataService::AccountPreviewPreference>&
        preference,
    const PromoLabelStringIds& ids) {
  if (!preference || preference->preferred_data_types.empty()) {
    return nil;
  }

  std::optional<std::u16string> device_type =
      GetDeviceTypeString(preference->other_device_info.form_factor);
  auto format_label = [&](int device_id, int all_devices_id) {
    return device_type.has_value()
               ? l10n_util::GetNSStringF(device_id, *device_type)
               : l10n_util::GetNSString(all_devices_id);
  };

  for (const signin::PreferredDataTypeInfo& data_type_info :
       preference->preferred_data_types) {
    if (!preference->other_device_info.enabled_data_types.Has(
            data_type_info.data_type)) {
      continue;
    }
    switch (data_type_info.data_type) {
      case syncer::BOOKMARKS:
        return format_label(ids.device_bookmarks, ids.all_devices_bookmarks);
      case syncer::PASSWORDS:
        return format_label(ids.device_passwords, ids.all_devices_passwords);
      case syncer::AUTOFILL:
      case syncer::AUTOFILL_WALLET_METADATA:
        return format_label(ids.device_saved_info, ids.all_devices_saved_info);
      default:
        break;
    }
  }
  return nil;
}

}  // namespace

namespace signin {

NSString* GetPromoLabelStringForWebSignin(
    const std::optional<AccountPreviewDataService::AccountPreviewPreference>&
        preference) {
  constexpr PromoLabelStringIds kWebSigninIds = {
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_BOOKMARKS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_BOOKMARKS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_PASSWORDS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_PASSWORDS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_SAVED_INFO,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_SAVED_INFO,
  };
  return GetPromoLabelStringWithIds(preference, kWebSigninIds);
}

NSString* GetPromoLabelStringForNtpSignedOutIcon(
    const std::optional<AccountPreviewDataService::AccountPreviewPreference>&
        preference) {
  constexpr PromoLabelStringIds kNtpIds = {
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_BOOKMARKS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_BOOKMARKS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_PASSWORDS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_PASSWORDS,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_SAVED_INFO,
      IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_SAVED_INFO,
  };
  return GetPromoLabelStringWithIds(preference, kNtpIds);
}

}  // namespace signin
