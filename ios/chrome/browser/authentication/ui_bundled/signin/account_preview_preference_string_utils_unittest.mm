// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/account_preview_preference_string_utils.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "components/signin/core/browser/account_preview_data_service.h"
#import "components/sync/base/data_type.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace signin {
namespace {

using AccountPreviewPreferenceStringUtilsTest = PlatformTest;

struct DataTypeTestCase {
  syncer::DataType data_type;
  int web_signin_string_id;
  int ntp_signin_string_id;
};

// Creates an `AccountPreviewPreference` with a single `data_type` and
// `form_factor`.
AccountPreviewDataService::AccountPreviewPreference CreatePreference(
    syncer::DataType data_type,
    sync_pb::SyncEnums_DeviceFormFactor form_factor) {
  AccountPreviewDataService::AccountPreviewPreference preference;
  preference.preferred_data_types.push_back({.data_type = data_type});
  preference.other_device_info.form_factor = form_factor;
  preference.other_device_info.enabled_data_types = {data_type};
  return preference;
}

// Tests that `GetPromoLabelStringForWebSignin` and
// `GetPromoLabelStringForNtpSignedOutIcon` return the expected string for all
// pairs of supported data types and device form factors.
TEST_F(AccountPreviewPreferenceStringUtilsTest,
       SupportedDataTypesAndDeviceTypesReturnExpectedString) {
  constexpr DataTypeTestCase kDataTypeCases[] = {
      {
          syncer::BOOKMARKS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_BOOKMARKS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_BOOKMARKS,
      },
      {
          syncer::PASSWORDS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_PASSWORDS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_PASSWORDS,
      },
      {
          syncer::AUTOFILL,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_SAVED_INFO,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_SAVED_INFO,
      },
      {
          syncer::AUTOFILL_WALLET_METADATA,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_DEVICE_TYPE_SAVED_INFO,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_DEVICE_TYPE_SAVED_INFO,
      },
  };

  struct DeviceTypeTestCase {
    sync_pb::SyncEnums_DeviceFormFactor form_factor;
    int device_type_string_id;
  };
  constexpr DeviceTypeTestCase kDeviceTypeCases[] = {
      {
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE,
          IDS_SIGNIN_DEVICE_TYPE_PHONE,
      },
      {
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET,
          IDS_SIGNIN_DEVICE_TYPE_TABLET,
      },
      {
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP,
          IDS_SIGNIN_DEVICE_TYPE_COMPUTER,
      },
  };

  for (const DataTypeTestCase& data_case : kDataTypeCases) {
    for (const DeviceTypeTestCase& device_case : kDeviceTypeCases) {
      AccountPreviewDataService::AccountPreviewPreference preference =
          CreatePreference(data_case.data_type, device_case.form_factor);
      std::u16string device_string =
          l10n_util::GetStringUTF16(device_case.device_type_string_id);

      EXPECT_NSEQ(GetPromoLabelStringForWebSignin(preference),
                  l10n_util::GetNSStringF(data_case.web_signin_string_id,
                                          device_string));
      EXPECT_NSEQ(GetPromoLabelStringForNtpSignedOutIcon(preference),
                  l10n_util::GetNSStringF(data_case.ntp_signin_string_id,
                                          device_string));
    }
  }
}

// Tests that `GetPromoLabelStringForWebSignin` and
// `GetPromoLabelStringForNtpSignedOutIcon` return `nil` when the data type is
// not in the supported categories.
TEST_F(AccountPreviewPreferenceStringUtilsTest, UnsupportedDataTypeReturnsNil) {
  constexpr syncer::DataType kUnsupportedDataTypes[] = {
      syncer::PREFERENCES,
      syncer::READING_LIST,
      syncer::HISTORY,
      syncer::SESSIONS,
  };

  for (syncer::DataType data_type : kUnsupportedDataTypes) {
    AccountPreviewDataService::AccountPreviewPreference preference =
        CreatePreference(
            data_type,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE);

    EXPECT_NSEQ(GetPromoLabelStringForWebSignin(preference), nil);
    EXPECT_NSEQ(GetPromoLabelStringForNtpSignedOutIcon(preference), nil);
  }
}

// Tests that `GetPromoLabelStringForWebSignin` and
// `GetPromoLabelStringForNtpSignedOutIcon` return the all-devices string for
// all supported data types when the device form factor is not phone, tablet, or
// laptop.
TEST_F(AccountPreviewPreferenceStringUtilsTest,
       OtherDeviceTypesReturnAllDevicesString) {
  constexpr DataTypeTestCase kDataTypeCases[] = {
      {
          syncer::BOOKMARKS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_BOOKMARKS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_BOOKMARKS,
      },
      {
          syncer::PASSWORDS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_PASSWORDS,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_PASSWORDS,
      },
      {
          syncer::AUTOFILL,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_SAVED_INFO,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_SAVED_INFO,
      },
      {
          syncer::AUTOFILL_WALLET_METADATA,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_SAVED_INFO,
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_SAVED_INFO,
      },
  };

  for (const DataTypeTestCase& data_case : kDataTypeCases) {
    AccountPreviewDataService::AccountPreviewPreference preference =
        CreatePreference(
            data_case.data_type,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED);

    EXPECT_NSEQ(GetPromoLabelStringForWebSignin(preference),
                l10n_util::GetNSString(data_case.web_signin_string_id));
    EXPECT_NSEQ(GetPromoLabelStringForNtpSignedOutIcon(preference),
                l10n_util::GetNSString(data_case.ntp_signin_string_id));
  }
}

// Tests that `GetPromoLabelStringForWebSignin` and
// `GetPromoLabelStringForNtpSignedOutIcon` return `nil` when `preference` is
// `std::nullopt` or has empty `preferred_data_types`.
TEST_F(AccountPreviewPreferenceStringUtilsTest, NoPreferenceReturnsNil) {
  EXPECT_NSEQ(GetPromoLabelStringForWebSignin(std::nullopt), nil);
  EXPECT_NSEQ(GetPromoLabelStringForNtpSignedOutIcon(std::nullopt), nil);

  AccountPreviewDataService::AccountPreviewPreference empty_preference;
  EXPECT_NSEQ(GetPromoLabelStringForWebSignin(empty_preference), nil);
  EXPECT_NSEQ(GetPromoLabelStringForNtpSignedOutIcon(empty_preference), nil);
}

// Tests that `GetPromoLabelStringForWebSignin` and
// `GetPromoLabelStringForNtpSignedOutIcon` skip preferred data types that are
// not in `other_device_info.enabled_data_types`, returning `nil` if no enabled
// type matches or falling back to the next enabled preferred data type.
TEST_F(AccountPreviewPreferenceStringUtilsTest,
       PreferredDataTypeNotInEnabledDataTypes) {
  AccountPreviewDataService::AccountPreviewPreference preference;
  preference.preferred_data_types.push_back({.data_type = syncer::BOOKMARKS});
  preference.other_device_info.enabled_data_types = {syncer::PASSWORDS};

  EXPECT_NSEQ(GetPromoLabelStringForWebSignin(preference), nil);
  EXPECT_NSEQ(GetPromoLabelStringForNtpSignedOutIcon(preference), nil);

  preference.preferred_data_types.push_back({.data_type = syncer::PASSWORDS});

  EXPECT_NSEQ(
      GetPromoLabelStringForWebSignin(preference),
      l10n_util::GetNSString(
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_FOR_WEB_SIGNIN_ON_ALL_DEVICES_PASSWORDS));
  EXPECT_NSEQ(
      GetPromoLabelStringForNtpSignedOutIcon(preference),
      l10n_util::GetNSString(
          IDS_SIGNIN_ACCOUNT_PICKER_BOTTOM_SHEET_SUBTITLE_ON_ALL_DEVICES_PASSWORDS));
}

}  // namespace
}  // namespace signin
