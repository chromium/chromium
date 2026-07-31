// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_IOS_DEVICE_IDENTIFIER_DELEGATE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_IOS_DEVICE_IDENTIFIER_DELEGATE_H_

#import <string>

// Delegate interface for retrieving device identifiers requiring
// iOS or Chrome iOS-specific APIs.
class IOSDeviceIdentifierDelegate {
 public:
  virtual ~IOSDeviceIdentifierDelegate() = default;

  // Returns the vendor identifier for the device.
  virtual std::string GetVendorId() = 0;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_IOS_DEVICE_IDENTIFIER_DELEGATE_H_
