// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/client_hints_controller_delegate.h"

namespace content {

bool ClientHintsControllerDelegate::ShouldForceEmptyViewportSize() {
  return false;
}

}  // namespace content
