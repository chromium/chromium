// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"

#import "ios/chrome/browser/shared/public/commands/application_commands.h"

SigninInProgress::SigninInProgress(id<SignInInProgressAudience> audience)
    : audience_(audience) {
  DCHECK(audience_);
  [audience_ signInStarted];
}

SigninInProgress::~SigninInProgress() {
  [audience_ signinFinished];
}
