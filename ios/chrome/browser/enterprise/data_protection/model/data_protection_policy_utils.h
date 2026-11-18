// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_POLICY_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_POLICY_UTILS_H_

class ProfileIOS;

// Returns true if the DisableScreenshots enterprise policy disables
// screenshots and screen recordings for `profile` (including Incognito).
// `profile` may be nullptr, in which case this returns false.
bool IsScreenshotsDisabledByPolicy(const ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_POLICY_UTILS_H_
