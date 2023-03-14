// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

namespace device {

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
      current_position_.error_code =
          mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
      current_position_.error_message.clear();
    }
    ReportCurrentPosition();
  }
}

void GeolocationImpl::PauseUpdates() {
  geolocation_subscription_ = {};
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
          base::BindRepeating(&GeolocationImpl::OnLocationUpdate,
                              base::Unretained(this)),
          high_accuracy_);
}

void GeolocationImpl::SetHighAccuracy(bool high_accuracy) {
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

  geolocation_subscription_ = {};

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
  DCHECK(context_);

  current_position_ = position;
  has_position_to_report_ = true;

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void GeolocationImpl::ReportCurrentPosition() {
  std::move(position_callback_).Run(current_position_.Clone());
  has_position_to_report_ = false;
}

}  // namespace device
