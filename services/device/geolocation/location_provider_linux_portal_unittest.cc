// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_linux_portal.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kLocationInterface[] = "org.freedesktop.portal.Location";

constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";

class LocationProviderLinuxPortalTest : public testing::Test {
 public:
  LocationProviderLinuxPortalTest()
      : mock_bus_(base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options())),
        mock_portal_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_bus_.get(),
            kPortalServiceName,
            dbus::ObjectPath(kPortalObjectPath))) {
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);

    ON_CALL(*mock_bus_, GetObjectProxy(kPortalServiceName, _))
        .WillByDefault(Return(mock_portal_proxy_.get()));

    ON_CALL(*mock_portal_proxy_,
            ConnectToSignal(kLocationInterface, "LocationUpdated", _, _))
        .WillByDefault(SaveArg<2>(&location_updated_callback_));

    ON_CALL(*mock_portal_proxy_,
            ConnectToSignal(kRequestInterface, "Response", _, _))
        .WillByDefault(
            [](const std::string& interface_name,
               const std::string& signal_name,
               dbus::ObjectProxy::SignalCallback signal_callback,
               dbus::ObjectProxy::OnConnectedCallback connected_callback) {
              std::move(connected_callback)
                  .Run(interface_name, signal_name, true);
            });

    ON_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillByDefault(
            [this](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              if (call->GetMember() == "CreateSession") {
                create_session_callback_ = std::move(callback);
              } else if (call->GetMember() == "Start") {
                start_callback_ = std::move(callback);
              }
            });
  }

  ~LocationProviderLinuxPortalTest() override = default;

 protected:
  void SimulateCreateSessionResponse(const std::string& session_path) {
    if (create_session_callback_.is_null()) {
      return;
    }
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendObjectPath(dbus::ObjectPath(session_path));
    std::move(create_session_callback_).Run(response.get(), nullptr);
  }

  void SimulateStartResponse(const std::string& handle_path) {
    if (start_callback_.is_null()) {
      return;
    }
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendObjectPath(dbus::ObjectPath(handle_path));
    std::move(start_callback_).Run(response.get(), nullptr);
  }

  void SimulateLocationUpdated(const std::string& session_path,
                               double latitude,
                               double longitude,
                               double accuracy) {
    if (location_updated_callback_.is_null()) {
      return;
    }
    dbus::Signal signal(kLocationInterface, "LocationUpdated");
    dbus::MessageWriter writer(&signal);
    writer.AppendObjectPath(dbus::ObjectPath(session_path));

    dbus::MessageWriter dict_writer(nullptr);
    writer.OpenArray("{sv}", &dict_writer);

    auto append_double_entry = [&](const std::string& key, double val) {
      dbus::MessageWriter entry_writer(nullptr);
      dict_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(key);
      entry_writer.AppendVariantOfDouble(val);
      dict_writer.CloseContainer(&entry_writer);
    };

    append_double_entry("Latitude", latitude);
    append_double_entry("Longitude", longitude);
    append_double_entry("Accuracy", accuracy);

    writer.CloseContainer(&dict_writer);

    location_updated_callback_.Run(&signal);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;

  dbus::ObjectProxy::ResponseOrErrorCallback create_session_callback_;
  dbus::ObjectProxy::ResponseOrErrorCallback start_callback_;
  dbus::ObjectProxy::SignalCallback location_updated_callback_;
};

TEST_F(LocationProviderLinuxPortalTest, CreateSessionAndStart) {
  device::LocationProviderLinuxPortal provider(mock_bus_);

  base::test::TestFuture<const device::LocationProvider*,
                         device::mojom::GeopositionResultPtr>
      location_update_future;
  provider.SetUpdateCallback(location_update_future.GetRepeatingCallback());

  provider.StartProvider(/*high_accuracy=*/true);
  provider.OnPermissionGranted();

  SimulateCreateSessionResponse("/org/freedesktop/portal/desktop/session/1");
  task_environment_.RunUntilIdle();
  SimulateStartResponse("/org/freedesktop/portal/desktop/request/1");

  SimulateLocationUpdated("/org/freedesktop/portal/desktop/session/1", 37.7749,
                          -122.4194, 10.0);

  auto [prov, result] = location_update_future.Take();
  ASSERT_TRUE(result->is_position());
  EXPECT_DOUBLE_EQ(result->get_position()->latitude, 37.7749);
  EXPECT_DOUBLE_EQ(result->get_position()->longitude, -122.4194);
  EXPECT_NE(provider.GetPosition(), nullptr);
}

TEST_F(LocationProviderLinuxPortalTest, RecreateSessionOnAccuracyChange) {
  device::LocationProviderLinuxPortal provider(mock_bus_);

  provider.StartProvider(/*high_accuracy=*/true);
  SimulateCreateSessionResponse("/org/freedesktop/portal/desktop/session/1");
  task_environment_.RunUntilIdle();
  SimulateStartResponse("/org/freedesktop/portal/desktop/request/1");

  // Now change the accuracy.
  provider.StartProvider(/*high_accuracy=*/false);

  // This should trigger recreation. Simulate the new responses.
  SimulateCreateSessionResponse("/org/freedesktop/portal/desktop/session/2");
  task_environment_.RunUntilIdle();
  SimulateStartResponse("/org/freedesktop/portal/desktop/request/2");

  provider.OnPermissionGranted();
  base::test::TestFuture<const device::LocationProvider*,
                         device::mojom::GeopositionResultPtr>
      location_update_future;
  provider.SetUpdateCallback(location_update_future.GetRepeatingCallback());

  SimulateLocationUpdated("/org/freedesktop/portal/desktop/session/2", 37.7749,
                          -122.4194, 10.0);

  auto [prov, result] = location_update_future.Take();
  ASSERT_TRUE(result->is_position());
  EXPECT_DOUBLE_EQ(result->get_position()->latitude, 37.7749);
}

TEST_F(LocationProviderLinuxPortalTest,
       InitializationFailurePortalNotAvailable) {
  device::LocationProviderLinuxPortal provider(mock_bus_);

  // Set up the portal state to Failed so that OnPortalRequested is called with
  // version 0.
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kFailed);

  base::test::TestFuture<const device::LocationProvider*,
                         device::mojom::GeopositionResultPtr>
      location_update_future;
  provider.SetUpdateCallback(location_update_future.GetRepeatingCallback());

  provider.StartProvider(/*high_accuracy=*/true);

  auto [prov, result] = location_update_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error()->error_code,
            device::mojom::GeopositionErrorCode::kPositionUnavailable);
}

TEST_F(LocationProviderLinuxPortalTest, InitializationFailureCreateSession) {
  device::LocationProviderLinuxPortal provider(mock_bus_);

  base::test::TestFuture<const device::LocationProvider*,
                         device::mojom::GeopositionResultPtr>
      location_update_future;
  provider.SetUpdateCallback(location_update_future.GetRepeatingCallback());

  provider.StartProvider(/*high_accuracy=*/true);

  // Simulate CreateSession error (passing null response).
  ASSERT_FALSE(create_session_callback_.is_null());
  std::move(create_session_callback_).Run(nullptr, nullptr);

  auto [prov, result] = location_update_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error()->error_code,
            device::mojom::GeopositionErrorCode::kPositionUnavailable);
}

TEST_F(LocationProviderLinuxPortalTest, DiagnosticsReflectState) {
  device::LocationProviderLinuxPortal provider(mock_bus_);
  device::mojom::GeolocationDiagnostics diag;

  provider.FillDiagnostics(diag);
  EXPECT_EQ(diag.provider_state,
            device::mojom::GeolocationDiagnostics::ProviderState::kStopped);

  provider.StartProvider(/*high_accuracy=*/true);
  provider.FillDiagnostics(diag);
  EXPECT_EQ(
      diag.provider_state,
      device::mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);

  provider.StartProvider(/*high_accuracy=*/false);
  provider.FillDiagnostics(diag);
  EXPECT_EQ(diag.provider_state,
            device::mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy);
}

TEST_F(LocationProviderLinuxPortalTest, StopProviderResetsState) {
  device::LocationProviderLinuxPortal provider(mock_bus_);
  provider.StartProvider(/*high_accuracy=*/true);
  provider.StopProvider();

  device::mojom::GeolocationDiagnostics diag;
  provider.FillDiagnostics(diag);
  EXPECT_EQ(diag.provider_state,
            device::mojom::GeolocationDiagnostics::ProviderState::kStopped);
}

}  // namespace
