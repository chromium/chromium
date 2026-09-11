// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_ATTESTATION_DEVICE_INTEGRITY_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_ATTESTATION_DEVICE_INTEGRITY_SERVICE_H_

#import <Foundation/Foundation.h>

#import <string>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"

// Purpose for requesting a device integrity snapshot.
enum class DeviceIntegrityPurpose {
  // Device authorization for password manager passkeys.
  kPasskeys,
};

// Service that provides device integrity snapshots.
class DeviceIntegrityService {
 public:
  // Parameters for requesting a snapshot.
  struct Params {
    DeviceIntegrityPurpose purpose;
    base::flat_map<std::string, std::string> content_bindings;
  };

  // Callback invoked with the device integrity snapshot.
  using SnapshotCallback = base::OnceCallback<void(NSData*)>;

  DeviceIntegrityService();
  DeviceIntegrityService(const DeviceIntegrityService&) = delete;
  DeviceIntegrityService& operator=(const DeviceIntegrityService&) = delete;
  virtual ~DeviceIntegrityService();

  // Asynchronously retrieves a device integrity snapshot configured by
  // `params`. Invokes `callback` with the snapshot on success, or `nil` on
  // failure.
  virtual void FetchSnapshot(Params params, SnapshotCallback callback) = 0;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DEVICE_ATTESTATION_DEVICE_INTEGRITY_SERVICE_H_
