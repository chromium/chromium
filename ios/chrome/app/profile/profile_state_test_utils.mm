// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state_test_utils.h"

#import "base/check_op.h"
#import "base/types/cxx23_to_underlying.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"

void SetProfileStateInitStage(ProfileState* profile_state,
                              ProfileInitStage init_stage) {
  ProfileInitStage curr_stage = profile_state.initStage;
  DCHECK_GE(init_stage, curr_stage);

  while (init_stage != curr_stage) {
    curr_stage =
        static_cast<ProfileInitStage>(base::to_underlying(curr_stage) + 1);

    profile_state.initStage = curr_stage;
  }
}
