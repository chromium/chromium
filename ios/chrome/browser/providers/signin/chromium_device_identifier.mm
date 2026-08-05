// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/device_util.h"
#import "ios/public/provider/chrome/browser/signin/device_identifier_api.h"

namespace ios::provider {

std::string GetDeviceIdentifier() {
  return ios::device_util::GetVendorId();
}

}  // namespace ios::provider
