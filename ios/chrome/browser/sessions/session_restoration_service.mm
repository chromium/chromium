// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service.h"

#import "base/check.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SessionRestorationService::SessionRestorationService() {
  DCHECK(web::features::UseSessionSerializationOptimizations());
}

SessionRestorationService::~SessionRestorationService() = default;

void SessionRestorationService::Shutdown() {}
