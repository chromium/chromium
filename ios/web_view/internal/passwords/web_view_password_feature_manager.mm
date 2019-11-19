// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/passwords/web_view_password_feature_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

bool WebViewPasswordFeatureManager::IsGenerationEnabled() const {
  return false;
}

bool WebViewPasswordFeatureManager::ShouldCheckReuseOnLeakDetection() const {
  return false;
}

}  // namespace ios_web_view
