// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/device_form_factor.h"

const char kIOSLensContextMenuSupportStatusHistogramName[] =
    "Mobile.ContextMenu.LensSupportStatus";
const char kIOSLensKeyboardSupportStatusHistogramName[] =
    "Mobile.Keyboard.LensSupportStatus";
const char kIOSLensComposeBoxSupportStatusHistogramName[] =
    "Mobile.ComposeBox.LensSupportStatus";
const char kIOSLensNewTabPageSupportStatusHistogramName[] =
    "Mobile.NewTabPage.LensSupportStatus";
const char kIOSSpotlightSupportStatusHistogramName[] =
    "Mobile.Spotlight.LensSupportStatus";
const char kIOSPlusButtonSupportStatusHistogramName[] =
    "Mobile.PlusButton.LensSupportStatus";

namespace lens_availability {

/// Returns the `LensSupportStatus` for the `entry_point` with
/// `is_google_default_search_engine`.
LensSupportStatus LensSupportStatusForLensEntryPoint(
    LensEntrypoint entry_point,
    bool is_google_default_search_engine) {
  if (!ios::provider::IsLensSupported()) {
    return LensSupportStatus::ProviderUnsupported;
  }
  if (!GetApplicationContext()->GetLocalState()->GetBoolean(
          prefs::kLensCameraAssistedSearchPolicyAllowed)) {
    return LensSupportStatus::DisabledByEnterprisePolicy;
  } else if (!is_google_default_search_engine) {
    return LensSupportStatus::NonGoogleSearchEngine;
  } else if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return LensSupportStatus::DeviceFormFactorTablet;
  } else if (base::FeatureList::IsEnabled(kDisableLensCamera)) {
    return LensSupportStatus::DisabledByFlag;
  } else {
    return LensSupportStatus::LensSearchSupported;
  }
}

/// Logs `lens_support_status` for the `entry_point`.
void LogLensSupportStatusForLensEntryPoint(
    LensEntrypoint entry_point,
    LensSupportStatus lens_support_status) {
  const char* availability_metric_name = nullptr;

  switch (entry_point) {
    case LensEntrypoint::ContextMenu:
      availability_metric_name = kIOSLensContextMenuSupportStatusHistogramName;
      break;
    case LensEntrypoint::Keyboard:
      availability_metric_name = kIOSLensKeyboardSupportStatusHistogramName;
      break;
    case LensEntrypoint::Composebox:
      availability_metric_name = kIOSLensComposeBoxSupportStatusHistogramName;
      break;
    case LensEntrypoint::NewTabPage:
      availability_metric_name = kIOSLensNewTabPageSupportStatusHistogramName;
      break;
    case LensEntrypoint::Spotlight:
      availability_metric_name = kIOSSpotlightSupportStatusHistogramName;
      break;
    case LensEntrypoint::PlusButton:
      availability_metric_name = kIOSPlusButtonSupportStatusHistogramName;
      break;
    case LensEntrypoint::HomeScreenWidget:
    case LensEntrypoint::AppIconLongPress:
      // App icon long press cannot log availailability.
      return;
    default:
      NOTREACHED() << "Unsupported Lens Entry Point.";
  }

  if (availability_metric_name) {
    base::UmaHistogramEnumeration(availability_metric_name,
                                  lens_support_status);
  }
}

bool CheckAvailabilityForLensEntryPoint(LensEntrypoint entry_point,
                                        bool is_google_default_search_engine) {
  return LensSupportStatusForLensEntryPoint(entry_point,
                                            is_google_default_search_engine) ==
         LensSupportStatus::LensSearchSupported;
}

bool CheckAndLogAvailabilityForLensEntryPoint(
    LensEntrypoint entry_point,
    BOOL is_google_default_search_engine) {
  LensSupportStatus lens_support_status = LensSupportStatusForLensEntryPoint(
      entry_point, is_google_default_search_engine);
  bool supported =
      lens_support_status == LensSupportStatus::LensSearchSupported;
  LogLensSupportStatusForLensEntryPoint(entry_point, lens_support_status);
  return supported;
}

}  // namespace lens_availability
