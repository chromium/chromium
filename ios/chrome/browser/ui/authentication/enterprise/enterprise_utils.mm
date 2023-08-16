// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"

#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

bool IsRestrictAccountsToPatternsEnabled() {
  return !GetApplicationContext()
              ->GetLocalState()
              ->GetList(prefs::kRestrictAccountsToPatterns)
              .empty();
}
