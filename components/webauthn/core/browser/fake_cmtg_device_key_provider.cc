// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/fake_cmtg_device_key_provider.h"

#include "base/check.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace webauthn {

FakeCmtgDeviceKeyProvider::RequestImpl::RequestImpl() = default;
FakeCmtgDeviceKeyProvider::RequestImpl::~RequestImpl() = default;

FakeCmtgDeviceKeyProvider::FakeCmtgDeviceKeyProvider() = default;
FakeCmtgDeviceKeyProvider::~FakeCmtgDeviceKeyProvider() = default;

std::unique_ptr<CmtgDeviceKeyProvider::Request>
FakeCmtgDeviceKeyProvider::GetDeviceKeys(Callback callback) {
  if (!hold_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeCmtgDeviceKeyProvider::DeliverResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
  return std::make_unique<RequestImpl>();
}

void FakeCmtgDeviceKeyProvider::SetNextKeys(
    std::vector<std::vector<uint8_t>> keys) {
  next_keys_ = std::move(keys);
  next_error_ = std::nullopt;
}

void FakeCmtgDeviceKeyProvider::SetNextError(std::optional<Error> error) {
  next_error_ = error;
  next_keys_ = std::nullopt;
}

void FakeCmtgDeviceKeyProvider::SetHoldCallback(bool hold) {
  hold_callback_ = hold;
}

void FakeCmtgDeviceKeyProvider::DeliverResult(Callback callback) {
  CHECK(next_error_.has_value() || next_keys_.has_value())
      << "Attempted to fetch CMTG device keys, but result not set.";
  if (next_error_.has_value()) {
    std::move(callback).Run(base::unexpected(*next_error_));
  } else {
    std::move(callback).Run(std::move(*next_keys_));
  }
  next_keys_ = std::nullopt;
  next_error_ = std::nullopt;
}

}  // namespace webauthn
