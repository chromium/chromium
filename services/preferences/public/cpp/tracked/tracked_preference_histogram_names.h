// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_TRACKED_PREFERENCE_HISTOGRAM_NAMES_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_TRACKED_PREFERENCE_HISTOGRAM_NAMES_H_

namespace user_prefs {
namespace tracked {

extern const char kTrackedPrefHistogramUnchanged[];
extern const char kTrackedPrefHistogramCleared[];
extern const char kTrackedPrefHistogramChanged[];
extern const char kTrackedPrefHistogramInitialized[];
extern const char kTrackedPrefHistogramTrustedInitialized[];
extern const char kTrackedPrefHistogramNullInitialized[];
extern const char kTrackedPrefHistogramWantedReset[];
extern const char kTrackedPrefHistogramReset[];
extern const char kTrackedPrefHistogramResetViaHmacFallback[];
extern const char kTrackedPrefHistogramResetEncrypted[];
extern const char kTrackedPrefRegistryValidationSuffix[];
extern const char kTrackedPrefHistogramWantedResetViaHmacFallback[];
extern const char kTrackedPrefHistogramWantedResetEncrypted[];
// Histograms for encrypted hash validation.
extern const char kTrackedPrefHistogramUnchangedEncrypted[];
extern const char kTrackedPrefHistogramClearedEncrypted[];
extern const char kTrackedPrefHistogramChangedEncrypted[];
// Histograms for HMAC validation fallback.
extern const char kTrackedPrefHistogramUnchangedViaHmacFallback[];
extern const char kTrackedPrefHistogramClearedViaHmacFallback[];
extern const char kTrackedPrefHistogramChangedViaHmacFallback[];

}  // namespace tracked
}  // namespace user_prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_TRACKED_TRACKED_PREFERENCE_HISTOGRAM_NAMES_H_
