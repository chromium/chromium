// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tab_resumption/public/tab_resumption_constants.h"

BASE_FEATURE(kIOSRemoteTabResumptionKillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

NSString* const kTabResumptionViewIdentifier = @"TabResumptionViewIdentifier";

const char kTabResumptionShowItemImmediately[] =
    "tab-resumption-show-item-immediately";
