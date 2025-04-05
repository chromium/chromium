// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_test_util.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/notreached.h"
#import "ios/chrome/app/change_profile_continuation.h"

namespace {
void NotReachedContinuationImpl(SceneState* scene_state,
                                base::OnceClosure closure) {
  NOTREACHED();
}

ChangeProfileContinuation NotReachedContinuationProviderImpl() {
  NOTREACHED();
}
}  // namespace

ChangeProfileContinuation NotReachedContinuation() {
  return base::BindOnce(&NotReachedContinuationImpl);
}

ChangeProfileContinuationProvider NotReachedContinuationProvider() {
  return base::BindRepeating(&NotReachedContinuationProviderImpl);
}
