// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/app_launcher/app_launcher_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kAppLauncherRefresh{"AppLauncherRefresh",
                                        base::FEATURE_ENABLED_BY_DEFAULT};
