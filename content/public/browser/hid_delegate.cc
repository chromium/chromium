// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/hid_delegate.h"

namespace content {

bool HidDelegate::IsKnownSecurityKey(
    BrowserContext* browser_context,
    const device::mojom::HidDeviceInfo& device) {
  return false;
}

}  // namespace content
