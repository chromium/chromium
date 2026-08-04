// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_linux_portal.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/request.h"
#include "components/dbus/xdg/session.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kLocationInterface[] = "org.freedesktop.portal.Location";

constexpr char kStartMethod[] = "Start";

constexpr char kLocationUpdatedSignal[] = "LocationUpdated";

// Location accuracy values matching org.freedesktop.portal.Location spec.
enum class LocationAccuracy : uint32_t {
  kNone = 0,
  kCountry = 1,
  kCity = 2,
  kNeighborhood = 3,
  kStreet = 4,
  kExact = 5,
};

}  // namespace

LocationProviderLinuxPortal::LocationProviderLinuxPortal(
    scoped_refptr<dbus::Bus> bus)
    : bus_(bus ? std::move(bus) : dbus_thread_linux::GetSharedSessionBus()) {}

LocationProviderLinuxPortal::~LocationProviderLinuxPortal() {
  StopProvider();
}

void LocationProviderLinuxPortal::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  if (!is_started_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kStopped;
  } else if (high_accuracy_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy;
  } else {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy;
  }
}

void LocationProviderLinuxPortal::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  callback_ = callback;
}

void LocationProviderLinuxPortal::StartProvider(bool high_accuracy) {
  if (is_started_) {
    if (high_accuracy_ == high_accuracy) {
      return;
    }
    StopProvider();
  }

  is_started_ = true;
  high_accuracy_ = high_accuracy;

  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(),
      base::BindOnce(&LocationProviderLinuxPortal::OnPortalRequested,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocationProviderLinuxPortal::StopProvider() {
  if (!is_started_) {
    return;
  }

  is_started_ = false;

  start_request_.reset();
  session_.reset();
  portal_proxy_ = nullptr;
}

const mojom::GeopositionResult* LocationProviderLinuxPortal::GetPosition() {
  return last_result_.get();
}

void LocationProviderLinuxPortal::OnPermissionGranted() {
  permission_granted_ = true;
  if (last_result_ && callback_) {
    callback_.Run(this, last_result_.Clone());
  }
}

void LocationProviderLinuxPortal::OnPortalRequested(uint32_t portal_version) {
  if (!is_started_) {
    return;
  }

  if (portal_version == 0) {
    last_result_ =
        mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
            mojom::GeopositionErrorCode::kPositionUnavailable, "", ""));
    if (callback_) {
      callback_.Run(this, last_result_.Clone());
    }
    return;
  }

  portal_proxy_ = bus_->GetObjectProxy(kPortalServiceName,
                                       dbus::ObjectPath(kPortalObjectPath));

  if (!is_signal_connected_) {
    is_signal_connected_ = true;
    dbus_utils::ConnectToSignal<"oa{sv}">(
        portal_proxy_, kLocationInterface, kLocationUpdatedSignal,
        base::BindRepeating(&LocationProviderLinuxPortal::OnLocationUpdated,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&LocationProviderLinuxPortal::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  dbus_xdg::Dictionary options;
  options["accuracy"] = dbus_utils::Variant::Wrap<"u">(static_cast<uint32_t>(
      high_accuracy_ ? LocationAccuracy::kExact : LocationAccuracy::kCity));

  session_ = dbus_xdg::Session::CreateDirect(
      bus_, portal_proxy_, kLocationInterface, std::move(options),
      base::BindOnce(&LocationProviderLinuxPortal::OnCreateSessionResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocationProviderLinuxPortal::OnCreateSessionResponse(
    dbus_xdg::Session* session) {
  if (!is_started_) {
    return;
  }

  if (!session) {
    DLOG(WARNING) << "Failed to create location portal session";
    session_.reset();
    last_result_ =
        mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
            mojom::GeopositionErrorCode::kPositionUnavailable, "", ""));
    if (callback_) {
      callback_.Run(this, last_result_.Clone());
    }
    return;
  }

  start_request_ = std::make_unique<dbus_xdg::Request>(
      bus_, portal_proxy_, kLocationInterface, kStartMethod,
      dbus_xdg::Dictionary(),
      base::BindOnce(&LocationProviderLinuxPortal::OnStartResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      session->path(), std::string(""));
}

void LocationProviderLinuxPortal::OnStartResponse(dbus_xdg::Results results) {
  if (!is_started_) {
    return;
  }

  if (!results.has_value()) {
    DLOG(WARNING) << "Failed to start location portal session";
    last_result_ =
        mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
            mojom::GeopositionErrorCode::kPositionUnavailable, "", ""));
    if (callback_) {
      callback_.Run(this, last_result_.Clone());
    }
  }
}

void LocationProviderLinuxPortal::OnLocationUpdated(
    dbus_utils::ConnectToSignalResultSig<"oa{sv}"> result) {
  if (!is_started_ || !result.has_value()) {
    return;
  }

  auto [session_path, location_dict] = std::move(*result);
  if (!session_ || session_path != session_->path()) {
    return;
  }

  mojom::Geoposition position;
  position.timestamp = base::Time::Now();

  auto get_double_field = [&](const char* key, double& field) {
    if (auto it = location_dict.find(key); it != location_dict.end()) {
      if (auto val = std::move(it->second).Take<double>(); val.has_value()) {
        field = *val;
      }
    }
  };

  get_double_field("Latitude", position.latitude);
  get_double_field("Longitude", position.longitude);
  get_double_field("Altitude", position.altitude);
  get_double_field("Accuracy", position.accuracy);
  get_double_field("Speed", position.speed);
  get_double_field("Heading", position.heading);

  if (!ValidateGeoposition(position)) {
    return;
  }

  last_result_ = mojom::GeopositionResult::NewPosition(position.Clone());
  if (permission_granted_ && callback_) {
    callback_.Run(this, last_result_.Clone());
  }
}

void LocationProviderLinuxPortal::OnSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool connected) {
  is_signal_connected_ = connected;
  if (!is_started_) {
    return;
  }

  if (!connected) {
    DLOG(ERROR) << "Failed to connect to " << interface_name << " for signal "
                << signal_name;
    last_result_ =
        mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
            mojom::GeopositionErrorCode::kPositionUnavailable, "", ""));
    if (callback_) {
      callback_.Run(this, last_result_.Clone());
    }
  }
}

std::unique_ptr<LocationProvider> NewSystemLocationProvider() {
  return std::make_unique<LocationProviderLinuxPortal>();
}

}  // namespace device
