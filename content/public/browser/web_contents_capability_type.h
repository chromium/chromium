// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_CAPABILITY_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_CAPABILITY_TYPE_H_

namespace content {

// Device activity types that can be used by a WebContents.
enum class WebContentsCapabilityType {
  // WebUSB
  kUSB,
  // Web Bluetooth
  kBluetoothConnected,
  kBluetoothScanning,
  // WebHID
  kHID,
  // Web Serial
  kSerial,
  // Geolocation
  kGeolocation
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_CAPABILITY_TYPE_H_
