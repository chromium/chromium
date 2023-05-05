// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/form_controls_browsertest_mac.h"

bool MacOSVersionSupportsDarkMode() {
  if (@available(macOS 10.14, *))
    return true;
  return false;
}
