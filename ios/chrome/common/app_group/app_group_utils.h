// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_UTILS_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_UTILS_H_

namespace app_group {

// Synchronously clears the `ApplicationGroup` and the `CommonApplicationGroup`
// app group sandbox (folder and NSUserDefaults).
// The function will be executed on the calling thread.
// Disclaimer: This method may delete data that were not created by Chrome. Its
// only purpose is to reset the application group to it's fresh install state.
// This method may take undetermined time as it will do file access on main
// thread and must only be called for testing purpose.
void ClearAppGroupSandbox();

}  // namespace app_group

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_UTILS_H_
