// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_availability.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/device_form_factor.h"

const char kIOSLensContextMenuSupportStatusHistogramName[] =
    "Mobile.ContextMenu.LensSupportStatus";
const char kIOSLensKeyboardSupportStatusHistogramName[] =
    "Mobile.Keyboard.LensSupportStatus";
const char kIOSLensNewTabPageSupportStatusHistogramName[] =
    "Mobile.NewTabPage.LensSupportStatus";
const char kIOSSpotlightSupportStatusHistogramName[] =
    "Mobile.Spotlight.LensSupportStatus";
const char kIOSPlusButtonSupportStatusHistogramName[] =
    "Mobile.PlusButton.LensSupportStatus";

namespace lens_availability {
bool CheckAndLogAvailabilityForLensEntryPoint(
    LensEntrypoint entry_point,
    BOOL is_google_default_search_engine) {
  const char* availability_metric_name = nullptr;

  switch (entry_point) {
    case LensEntrypoint::ContextMenu:
      availability_metric_name = kIOSLensContextMenuSupportStatusHistogramName;
      break;
    case LensEntrypoint::Keyboard:
      availability_metric_name = kIOSLensKeyboardSupportStatusHistogramName;
      break;
    case LensEntrypoint::NewTabPage:
      availability_metric_name = kIOSLensNewTabPageSupportStatusHistogramName;
      break;
    case LensEntrypoint::HomeScreenWidget:
      // Home screen widget cannot log availailability.
      break;
    case LensEntrypoint::AppIconLongPress:
      // App icon long press cannot log availailability.
      break;
    case LensEntrypoint::Spotlight:
      availability_metric_name = kIOSSpotlightSupportStatusHistogramName;
      break;
    case LensEntrypoint::PlusButton:
      availability_metric_name = kIOSPlusButtonSupportStatusHistogramName;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported Lens Entry Point.";
  }

  LensSupportStatus lens_support_status;
  if (!ios::provider::IsLensSupported()) {
    lens_support_status = LensSupportStatus::ProviderUnsupported;
  } else if (!GetApplicationContext()->GetLocalState()->GetBoolean(
                 prefs::kLensCameraAssistedSearchPolicyAllowed)) {
    lens_support_status = LensSupportStatus::DisabledByEnterprisePolicy;
  } else if (!is_google_default_search_engine) {
    lens_support_status = LensSupportStatus::NonGoogleSearchEngine;
  } else if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    lens_support_status = LensSupportStatus::DeviceFormFactorTablet;
  } else if (base::FeatureList::IsEnabled(kDisableLensCamera)) {
    lens_support_status = LensSupportStatus::DisabledByFlag;
  } else {
    lens_support_status = LensSupportStatus::LensSearchSupported;
  }
  if (availability_metric_name) {
    base::UmaHistogramEnumeration(availability_metric_name,
                                  lens_support_status);
  }

  return lens_support_status == LensSupportStatus::LensSearchSupported;
}
}  // namespace lens_availability
