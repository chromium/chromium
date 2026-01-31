// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

namespace device {

namespace {
void RecordUmaGeolocationImplClientId(mojom::GeolocationClientId client_id) {
  base::UmaHistogramEnumeration("Geolocation.GeolocationImpl.ClientId",
                                client_id);
}
}  // namespace

GeolocationImpl::GeolocationImpl(mojo::PendingReceiver<Geolocation> receiver,
                                 const GURL& requesting_url,
                                 mojom::GeolocationClientId client_id,
                                 GeolocationContext* context,
                                 bool has_precise_permission)
    : receiver_(this, std::move(receiver)),
      url_(requesting_url),
      client_id_(client_id),
      context_(context),
      high_accuracy_hint_(false),
      has_precise_permission_(has_precise_permission) {
  DCHECK(context_);
  receiver_.set_disconnect_handler(base::BindOnce(
      &GeolocationImpl::OnConnectionError, base::Unretained(this)));
}

GeolocationImpl::~GeolocationImpl() {
  // Make sure to respond to any pending callback even without a valid position.
  if (!position_callback_.is_null()) {
    if (!current_result_ || !current_result_->is_error()) {
      current_result_ =
          mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
              mojom::GeopositionErrorCode::kPositionUnavailable,
              /*error_message=*/"", /*error_technical=*/""));
    }
    ReportCurrentPosition();
  }
}

void GeolocationImpl::PauseUpdates() {
  geolocation_subscription_ = {};
}

void GeolocationImpl::ResumeUpdates() {
  if (position_override_) {
    OnLocationUpdate(*position_override_);
    return;
  }

  StartListeningForUpdates();
}

void GeolocationImpl::StartListeningForUpdates() {
  const bool effective_high_accuracy =
      high_accuracy_hint_ && has_precise_permission_;

  if (effective_high_accuracy_ != effective_high_accuracy) {
    effective_high_accuracy_ = effective_high_accuracy;
    // When the accuracy requirement changes, we should reset `current_result_`
    // so we will not report a stale position.
    current_result_.reset();
    // `geolocation_subscription_` is not explicitly reset here. Allowing a
    // short period of concurrent high/low accuracy subscriptions is preferred
    // over stop/start transitions that exposed crbug.com/469328127.
    // `GeolocationProviderImpl::OnClientsChanged()` handles client priority
    // based on `kApproximateGeolocationPermission`.
    geolocation_subscription_ =
        GeolocationProvider::GetInstance()->AddLocationUpdateCallback(
            base::BindRepeating(&GeolocationImpl::OnLocationUpdate,
                                base::Unretained(this)),
            *effective_high_accuracy_);
  }
}

void GeolocationImpl::SetHighAccuracyHint(bool high_accuracy) {
  high_accuracy_hint_ = high_accuracy;

  if (position_override_) {
    OnLocationUpdate(*position_override_);
    return;
  }

  StartListeningForUpdates();
}

void GeolocationImpl::QueryNextPosition(QueryNextPositionCallback callback) {
  if (!position_callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextPosition!";
    OnConnectionError();  // Simulate a connection error.
    return;
  }

  position_callback_ = std::move(callback);

  if (current_result_) {
    ReportCurrentPosition();
  }
  RecordUmaGeolocationImplClientId(client_id_);
}

void GeolocationImpl::SetOverride(const mojom::GeopositionResult& result) {
  if (!position_callback_.is_null()) {
    if (!current_result_) {
      current_result_ =
          mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
              mojom::GeopositionErrorCode::kPositionUnavailable,
              /*error_message=*/"", /*error_technical=*/""));
    }
    ReportCurrentPosition();
  }

  position_override_ = result.Clone();
  if (result.is_error() ||
      (result.is_position() && !ValidateGeoposition(*result.get_position()))) {
    ResumeUpdates();
  }

  geolocation_subscription_ = {};

  OnLocationUpdate(*position_override_);
}

void GeolocationImpl::ClearOverride() {
  position_override_.reset();
  StartListeningForUpdates();
}

void GeolocationImpl::OnPermissionUpdated(
    mojom::GeolocationPermissionLevel permission_level) {
  if (permission_level == mojom::GeolocationPermissionLevel::kDenied) {
    if (!position_callback_.is_null()) {
      std::move(position_callback_)
          .Run(mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
              mojom::GeopositionErrorCode::kPermissionDenied,
              /*error_message=*/"User denied Geolocation",
              /*error_technical=*/"")));
      position_callback_.Reset();
    }
    geolocation_subscription_ = {};
  } else {
    has_precise_permission_ =
        (permission_level == mojom::GeolocationPermissionLevel::kPrecise);
    StartListeningForUpdates();
  }
}

void GeolocationImpl::OnConnectionError() {
  context_->OnConnectionError(this);

  // The above call deleted this instance, so the only safe thing to do is
  // return.
}

void GeolocationImpl::OnLocationUpdate(const mojom::GeopositionResult& result) {
  DCHECK(context_);

  current_result_ = result.Clone();

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void GeolocationImpl::ReportCurrentPosition() {
  CHECK(current_result_);
  std::move(position_callback_).Run(std::move(current_result_));
}

}  // namespace device
