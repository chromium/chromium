// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IPH_FOR_NEW_CHROME_USER_MODEL_UTILS_H_
#define IOS_CHROME_BROWSER_IPH_FOR_NEW_CHROME_USER_MODEL_UTILS_H_

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}

namespace iph_for_new_chrome_user {

// Returns eligibility of seeing the IPH for Safari Switcher for new chrome
// users. The critieria: 1. device switcher classifies it as the first syncing
// device.
// 2. the time since FRE is within a certain range.
bool IsUserNewSafariSwitcher(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher);

}  // namespace iph_for_new_chrome_user

#endif  // IOS_CHROME_BROWSER_IPH_FOR_NEW_CHROME_USER_MODEL_UTILS_H_
