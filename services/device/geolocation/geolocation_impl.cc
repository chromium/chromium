// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

namespace device {

namespace {

// Geoposition error codes for reporting in UMA.
enum GeopositionErrorCode {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.

  // There was no error.
  GEOPOSITION_ERROR_CODE_NONE = 0,

  // User denied use of geolocation.
  GEOPOSITION_ERROR_CODE_PERMISSION_DENIED = 1,

  // Geoposition could not be determined.
  GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE = 2,

  // Timeout.
  GEOPOSITION_ERROR_CODE_TIMEOUT = 3,

  // NOTE: Add entries only immediately above this line.
  GEOPOSITION_ERROR_CODE_COUNT = 4
};

void RecordGeopositionErrorCode(mojom::Geoposition::ErrorCode error_code) {
  GeopositionErrorCode code = GEOPOSITION_ERROR_CODE_NONE;
  switch (error_code) {
    case mojom::Geoposition::ErrorCode::NONE:
      code = GEOPOSITION_ERROR_CODE_NONE;
      break;
    case mojom::Geoposition::ErrorCode::PERMISSION_DENIED:
      code = GEOPOSITION_ERROR_CODE_PERMISSION_DENIED;
      break;
    case mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE:
      code = GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE;
      break;
    case mojom::Geoposition::ErrorCode::TIMEOUT:
      code = GEOPOSITION_ERROR_CODE_TIMEOUT;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Geolocation.LocationUpdate.ErrorCode", code,
                            GEOPOSITION_ERROR_CODE_COUNT);
}

}  // namespace

GeolocationImpl::GeolocationImpl(mojo::PendingReceiver<Geolocation> receiver,
                                 GeolocationContext* context)
    : receiver_(this, std::move(receiver)),
      context_(context),
      high_accuracy_(false),
      has_position_to_report_(false) {
  DCHECK(context_);
  receiver_.set_disconnect_handler(base::BindOnce(
      &GeolocationImpl::OnConnectionError, base::Unretained(this)));
}

GeolocationImpl::~GeolocationImpl() {
  // Make sure to respond to any pending callback even without a valid position.
  if (!position_callback_.is_null()) {
    if (ValidateGeoposition(current_position_)) {
      current_position_.error_code = mojom::Geoposition::ErrorCode(
          GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE);
      current_position_.error_message.clear();
    }
    ReportCurrentPosition();
  }
}

void GeolocationImpl::PauseUpdates() {
  geolocation_subscription_.reset();
}

void GeolocationImpl::ResumeUpdates() {
  if (ValidateGeoposition(position_override_)) {
    OnLocationUpdate(position_override_);
    return;
  }

  StartListeningForUpdates();
}

void GeolocationImpl::StartListeningForUpdates() {
  geolocation_subscription_ =
      GeolocationProvider::GetInstance()->AddLocationUpdateCallback(
          base::Bind(&GeolocationImpl::OnLocationUpdate,
                     base::Unretained(this)),
          high_accuracy_);
}

void GeolocationImpl::SetHighAccuracy(bool high_accuracy) {
  UMA_HISTOGRAM_BOOLEAN(
      "Geolocation.GeolocationDispatcherHostImpl.EnableHighAccuracy",
      high_accuracy);
  high_accuracy_ = high_accuracy;

  if (ValidateGeoposition(position_override_)) {
    OnLocationUpdate(position_override_);
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

  if (has_position_to_report_)
    ReportCurrentPosition();
}

void GeolocationImpl::SetOverride(const mojom::Geoposition& position) {
  if (!position_callback_.is_null())
    ReportCurrentPosition();
  position_override_ = position;
  if (!ValidateGeoposition(position_override_))
    ResumeUpdates();

  geolocation_subscription_.reset();

  OnLocationUpdate(position_override_);
}

void GeolocationImpl::ClearOverride() {
  position_override_ = mojom::Geoposition();
  StartListeningForUpdates();
}

void GeolocationImpl::OnConnectionError() {
  context_->OnConnectionError(this);

  // The above call deleted this instance, so the only safe thing to do is
  // return.
}

void GeolocationImpl::OnLocationUpdate(const mojom::Geoposition& position) {
  RecordGeopositionErrorCode(position.error_code);
  DCHECK(context_);

  current_position_ = position;
  current_position_.valid = ValidateGeoposition(position);
  has_position_to_report_ = true;

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void GeolocationImpl::ReportCurrentPosition() {
  std::move(position_callback_).Run(current_position_.Clone());
  has_position_to_report_ = false;
}

}  // namespace device
