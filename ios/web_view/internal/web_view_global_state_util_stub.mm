// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_global_state_util.h"

namespace ios_web_view {

void InitializeGlobalState() {
  // Do not perform global state initialization in an unit test environment.
  // It's not needed and also avoids the issue of multiple AtExitManagers.
}

}  // namespace ios_web_view
