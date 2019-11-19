// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace tests_hook {

bool DisableAppGroupAccess() {
  return false;
}
bool DisableContentSuggestions() {
  return false;
}
bool DisableFirstRun() {
  return false;
}
bool DisableGeolocation() {
  return false;
}
bool DisableSigninRecallPromo() {
  return false;
}
bool DisableUpdateService() {
  return false;
}
void SetUpTestsIfPresent() {}
void RunTestsIfPresent() {}

}  // namespace tests_hook
