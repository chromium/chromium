// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/tracked/tracked_preference_histogram_names.h"

namespace user_prefs {
namespace tracked {

// Tracked pref histogram names.
const char kTrackedPrefHistogramUnchanged[] =
    "Settings.TrackedPreferenceUnchanged";
const char kTrackedPrefHistogramCleared[] = "Settings.TrackedPreferenceCleared";
const char kTrackedPrefHistogramChanged[] = "Settings.TrackedPreferenceChanged";
const char kTrackedPrefHistogramInitialized[] =
    "Settings.TrackedPreferenceInitialized";
const char kTrackedPrefHistogramTrustedInitialized[] =
    "Settings.TrackedPreferenceTrustedInitialized";
const char kTrackedPrefHistogramNullInitialized[] =
    "Settings.TrackedPreferenceNullInitialized";
const char kTrackedPrefHistogramWantedReset[] =
    "Settings.TrackedPreferenceWantedReset";
const char kTrackedPrefHistogramReset[] = "Settings.TrackedPreferenceReset";
const char kTrackedPrefHistogramResetViaHmacFallback[] =
    "Settings.TrackedPreferenceResetViaHmacFallback";
const char kTrackedPrefHistogramResetEncrypted[] =
    "Settings.TrackedPreferenceResetEncrypted";
const char kTrackedPrefRegistryValidationSuffix[] = "FromRegistry";

const char kTrackedPrefHistogramWantedResetViaHmacFallback[] =
    "Settings.TrackedPreferenceWantedResetViaHmacFallback";
const char kTrackedPrefHistogramWantedResetEncrypted[] =
    "Settings.TrackedPreferenceWantedResetEncrypted";

const char kTrackedPrefHistogramUnchangedEncrypted[] =
    "Settings.TrackedPreferenceUnchangedEncrypted";
const char kTrackedPrefHistogramClearedEncrypted[] =
    "Settings.TrackedPreferenceClearedEncrypted";
const char kTrackedPrefHistogramChangedEncrypted[] =
    "Settings.TrackedPreferenceChangedEncrypted";

const char kTrackedPrefHistogramUnchangedViaHmacFallback[] =
    "Settings.TrackedPreferenceUnchangedViaHmacFallback";
const char kTrackedPrefHistogramClearedViaHmacFallback[] =
    "Settings.TrackedPreferenceClearedViaHmacFallback";
const char kTrackedPrefHistogramChangedViaHmacFallback[] =
    "Settings.TrackedPreferenceChangedViaHmacFallback";
}  // namespace tracked
}  // namespace user_prefs
