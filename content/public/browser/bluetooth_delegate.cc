// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/bluetooth_delegate.h"

namespace content {

BluetoothDelegate::AllowWebBluetoothResult BluetoothDelegate::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return BluetoothDelegate::AllowWebBluetoothResult::kAllow;
}

std::string BluetoothDelegate::GetWebBluetoothBlocklist() {
  return std::string();
}

bool BluetoothDelegate::IsBluetoothScanningBlocked(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return false;
}

}  // namespace content
