// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"

#import "base/functional/callback.h"
#import "base/functional/callback_forward.h"

namespace {

void DoNothingContinuationImpl(SceneState* scene_state,
                               base::OnceClosure closure) {
  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation DoNothingContinuation() {
  return base::BindOnce(&DoNothingContinuationImpl);
}
