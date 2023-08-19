// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_restoration_service.h"

#include "base/check.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

SessionRestorationService::SessionRestorationService() {
  DCHECK(web::features::UseSessionSerializationOptimizations());
}

SessionRestorationService::~SessionRestorationService() = default;
