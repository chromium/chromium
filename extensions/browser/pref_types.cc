// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_types.h"

namespace extensions {
// Records the number of corrupted extensions that have been disabled.
const PrefMap kCorruptedDisableCount = {"extensions.corrupted_disable_count",
                                        PrefType::kInteger,
                                        PrefScope::kProfile};

// Records the user permissions.
const PrefMap kUserPermissions = {"extensions.user_permissions",
                                  PrefType::kDictionary, PrefScope::kProfile};

// Records the configuration of user scripts worlds.
const PrefMap kUserScriptsWorldsConfiguration = {
    "user_scripts_worlds.configuration", PrefType::kDictionary,
    PrefScope::kExtensionSpecific};

// Stores whether the user has acknowledged the MV2 deprecation notice for the
// warning stage globally.
const PrefMap kMV2DeprecationWarningAcknowledgedGloballyPref = {
    "mv2_deprecation_warning_ack_globally", PrefType::kBool,
    PrefScope::kProfile};

// Stores whether the user has acknowledged the MV2 deprecation notice for the
// disabled stage globally.
const PrefMap kMV2DeprecationDisabledAcknowledgedGloballyPref = {
    "mv2_deprecation_disabled_ack_globally", PrefType::kBool,
    PrefScope::kProfile};

// Stores whether the user has acknowledged the MV2 deprecation notice for the
// unsupported stage globally.
const PrefMap kMV2DeprecationUnsupportedAcknowledgedGloballyPref = {
    "mv2_deprecation_unsupported_ack_globally", PrefType::kBool,
    PrefScope::kProfile};

}  // namespace extensions
