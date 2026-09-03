// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/device_attestation/device_attestation_api.h"

namespace ios::provider {

std::unique_ptr<enterprise::AttestationServiceIOS>
CreateAttestationServiceIOS() {
  return nullptr;
}

}  // namespace ios::provider
