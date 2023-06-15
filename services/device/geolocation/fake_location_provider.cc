// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a fake location provider and the factory functions for
// various ways of creating it.
// TODO(lethalantidote): Convert location_arbitrator_impl to use actual mock
// instead of FakeLocationProvider.

#include "services/device/geolocation/fake_location_provider.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace device {

FakeLocationProvider::FakeLocationProvider()
    : provider_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
}

FakeLocationProvider::~FakeLocationProvider() = default;

void FakeLocationProvider::HandlePositionChanged(
    mojom::GeopositionResultPtr result) {
  if (provider_task_runner_->BelongsToCurrentThread()) {
    // The location arbitrator unit tests rely on this method running
    // synchronously.
    result_ = std::move(result);
    if (!callback_.is_null())
      callback_.Run(this, result_.Clone());
  } else {
    provider_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeLocationProvider::HandlePositionChanged,
                                  base::Unretained(this), std::move(result)));
  }
}

void FakeLocationProvider::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  diagnostics.provider_state = state_;
}

void FakeLocationProvider::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  callback_ = callback;
}

void FakeLocationProvider::StartProvider(bool high_accuracy) {
  state_ = high_accuracy
               ? mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy
               : mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy;
}

void FakeLocationProvider::StopProvider() {
  state_ = mojom::GeolocationDiagnostics::ProviderState::kStopped;
}

const mojom::GeopositionResult* FakeLocationProvider::GetPosition() {
  return result_.get();
}

void FakeLocationProvider::OnPermissionGranted() {
  is_permission_granted_ = true;
}

}  // namespace device
