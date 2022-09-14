// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_SETTINGS_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_SETTINGS_TEST_UTIL_H_

#include "components/content_settings/core/common/content_settings.h"

namespace chrome_test_util {

// Sets value for content setting.
void SetContentSettingsBlockPopups(ContentSetting setting);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_SETTINGS_TEST_UTIL_H_
