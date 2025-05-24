// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"

#import "base/functional/callback.h"
#import "base/functional/callback_forward.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"

namespace {

void DoNothingContinuationImpl(SceneState* scene_state,
                               base::OnceClosure closure) {
  std::move(closure).Run();
}

ChangeProfileContinuation DoNothingContinuationImplProvider() {
  return DoNothingContinuation();
}

}  // namespace

ChangeProfileContinuation DoNothingContinuation() {
  return base::BindOnce(&DoNothingContinuationImpl);
}

ChangeProfileContinuationProvider DoNothingContinuationProvider() {
  return base::BindRepeating(&DoNothingContinuationImplProvider);
}
