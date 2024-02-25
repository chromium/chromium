// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_FIELD_TRIAL_VERSION_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_FIELD_TRIAL_VERSION_H_

#import <Foundation/Foundation.h>

// We want to store the value of flags for some trials so they can be read
// in extensions. If the version of the trial when the flag value was stored
// doesn't match the current version of the trial, that flag value is invalid
// and the extension should treat the trial's value as disabled.

// The dictionary key for the trial value.
extern NSString* const kFieldTrialValueKey;
// The dictionary key for the trial version.
extern NSString* const kFieldTrialVersionKey;

// The current version of the credential provider extension's password creation
// feature.
extern const int kPasswordCreationFeatureVersion;

// The current version of the credential provider extension's favicon display
// feature.
extern const int kCredentialProviderExtensionFaviconsFeatureVersion;

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_FIELD_TRIAL_VERSION_H_
