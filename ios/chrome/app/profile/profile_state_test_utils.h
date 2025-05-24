// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_TEST_UTILS_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_TEST_UTILS_H_

enum class ProfileInitStage;
@class ProfileState;

// Sets the init stage of `profile_state` to `init_stage`. It is an error to
// call this function with a value that is smaller than the current init stage.
void SetProfileStateInitStage(ProfileState* profile_state,
                              ProfileInitStage init_stage);

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_TEST_UTILS_H_
