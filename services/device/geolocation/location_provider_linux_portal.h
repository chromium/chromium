// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_LINUX_PORTAL_H_
#define SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_LINUX_PORTAL_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/utils/variant.h"
#include "dbus/object_path.h"
#include "services/device/public/cpp/geolocation/location_provider.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

namespace dbus_xdg {
class Request;
class Session;
enum class ResponseError;
using Dictionary = std::map<std::string, dbus_utils::Variant>;
using Results = base::expected<Dictionary, ResponseError>;
}  // namespace dbus_xdg

namespace device {

// LocationProvider implementation for Linux desktops using XDG Desktop Portal
// (org.freedesktop.portal.Location).
class LocationProviderLinuxPortal : public LocationProvider {
 public:
  explicit LocationProviderLinuxPortal(scoped_refptr<dbus::Bus> bus = nullptr);
  LocationProviderLinuxPortal(const LocationProviderLinuxPortal&) = delete;
  LocationProviderLinuxPortal& operator=(const LocationProviderLinuxPortal&) =
      delete;
  ~LocationProviderLinuxPortal() override;

  // LocationProvider implementation:
  void FillDiagnostics(mojom::GeolocationDiagnostics& diagnostics) override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 private:
  void OnPortalRequested(uint32_t portal_version);
  void OnCreateSessionResponse(dbus_xdg::Session* session);
  void OnStartResponse(dbus_xdg::Results results);
  void OnLocationUpdated(dbus_utils::ConnectToSignalResultSig<"oa{sv}"> result);
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected);

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> portal_proxy_ = nullptr;
  std::unique_ptr<dbus_xdg::Session> session_;
  std::unique_ptr<dbus_xdg::Request> start_request_;
  bool is_signal_connected_ = false;
  bool is_started_ = false;
  bool high_accuracy_ = false;
  bool permission_granted_ = false;
  LocationProviderUpdateCallback callback_;
  mojom::GeopositionResultPtr last_result_;

  base::WeakPtrFactory<LocationProviderLinuxPortal> weak_ptr_factory_{this};
};

std::unique_ptr<LocationProvider> NewSystemLocationProvider();

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_LINUX_PORTAL_H_
