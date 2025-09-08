// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"

#import "base/time/time.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"

SnackbarMessage* CreateCustomSnackbarMessage(NSString* text) {
  SnackbarMessage* snackbar_message =
      [[SnackbarMessage alloc] initWithTitle:text];
  base::TimeDelta overridden_duration =
      tests_hook::GetOverriddenSnackbarDuration();
  if (overridden_duration.InSeconds() != 0) {
    snackbar_message.duration = overridden_duration.InSeconds();
  }
  return snackbar_message;
}
