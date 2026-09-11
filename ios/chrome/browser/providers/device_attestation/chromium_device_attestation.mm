// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/public/provider/chrome/browser/device_attestation/device_attestation_api.h"

namespace ios::provider {
namespace {

class ChromiumDeviceIntegrityService final : public DeviceIntegrityService {
 public:
  ChromiumDeviceIntegrityService() = default;
  ~ChromiumDeviceIntegrityService() override = default;

  void FetchSnapshot(Params params, SnapshotCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nil));
  }
};

}  // namespace

std::unique_ptr<enterprise::AttestationServiceIOS>
CreateAttestationServiceIOS() {
  return nullptr;
}

std::unique_ptr<DeviceIntegrityService> CreateDeviceIntegrityService() {
  return std::make_unique<ChromiumDeviceIntegrityService>();
}

}  // namespace ios::provider
