// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_FEATURES_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable the Fullscreen Promos Manager.
// For more information, please see here:
// go/bling-fullscreen-promos-manager-design-doc.
extern const base::Feature kFullscreenPromosManager;

// Returns true if the Fullscreen Promos Manager is enabled.
bool IsFullscreenPromosManagerEnabled();

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_FEATURES_H_
