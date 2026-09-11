// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_ATTESTATION_DEVICE_ATTESTATION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_ATTESTATION_DEVICE_ATTESTATION_API_H_

#import <memory>

#import "components/enterprise/device_attestation/ios/attestation_service_ios.h"
#import "ios/public/provider/chrome/browser/device_attestation/device_integrity_service.h"

namespace ios::provider {

// Creates an instance of `AttestationServiceIOS`.
std::unique_ptr<enterprise::AttestationServiceIOS>
CreateAttestationServiceIOS();

// Creates an instance of `DeviceIntegrityService`.
std::unique_ptr<DeviceIntegrityService> CreateDeviceIntegrityService();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_ATTESTATION_DEVICE_ATTESTATION_API_H_
