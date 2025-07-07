// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_UTILS_H_

class Browser;
@protocol SystemIdentity;

// Displays the identity confirmation snackbar with `identity`.
void TriggerAccountSwitchSnackbarWithIdentity(id<SystemIdentity> identity,
                                              Browser* browser);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_UTILS_H_
