// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_global_state_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

void InitializeGlobalState() {
  // Do not perform global state initialization in an unit test environment.
  // It's not needed and also avoids the issue of multiple AtExitManagers.
}

}  // namespace ios_web_view
