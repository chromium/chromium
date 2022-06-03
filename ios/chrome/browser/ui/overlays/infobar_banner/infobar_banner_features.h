// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_FEATURES_H_

#include "base/feature_list.h"

// The feature parameter that indicates the duration of a long presentation
// message.
extern const char kLongPresentationMessagesDurationFeatureParam[];

// The feature parameter that indicates the duration of a default
// presentation message.
extern const char kDefaultPresentationMessagesDurationFeatureParam[];

// The feature to enable long message duration
extern const base::Feature kEnableLongMessageDuration;

// Checks whether the long message duration feature is enabled.
bool IsLongMessageDurationEnabled();

// Returns the duration of the default presentation messages.
double GetDefaultPresentationMessageDuration();

// Returns the duration of the long presentation messages.
double GetLongPresentationMessageDuration();

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_FEATURES_H_
