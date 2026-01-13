// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_FEATURES_H_
#define IOS_CHROME_APP_PROFILE_FEATURES_H_

#import "base/feature_list.h"

BASE_DECLARE_FEATURE(kLogApplicationStorageSizeMetrics);

// Feature to disable CookieStoreIOS::FlushStore when the application enters the
// background.
BASE_DECLARE_FEATURE(kDisableCookieStoreIOSFlushOnBackgrounding);

#endif  // IOS_CHROME_APP_PROFILE_FEATURES_H_
