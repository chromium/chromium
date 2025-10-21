// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_linux.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace device {

namespace {

const char kNetworkManagerServiceName[] = "org.freedesktop.NetworkManager";
const char kNetworkManagerPath[] = "/org/freedesktop/NetworkManager";
const char kDevicePath[] = "/org/freedesktop/NetworkManager/Devices/0";
const char kDevicePath2[] = "/org/freedesktop/NetworkManager/Devices/1";
const char kAccessPointPath[] = "/org/freedesktop/NetworkManager/AccessPoint/0";
const char kAccessPointPath2[] =
    "/org/freedesktop/NetworkManager/AccessPoint/1";
const char kAccessPointPath3[] =
    "/org/freedesktop/NetworkManager/AccessPoint/2";

}  // namespace

class GeolocationWifiDataProviderLinuxTest : public testing::Test {
 public:
  GeolocationWifiDataProviderLinuxTest() = default;

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(std::move(options));
    EXPECT_CALL(*mock_bus_, Connect()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_bus_, GetOriginTaskRunner())
        .WillRepeatedly(
            Return(base::SequencedTaskRunner::GetCurrentDefault().get()));

    // Create a mock proxy for NetworkManager.
    mock_network_manager_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kNetworkManagerServiceName,
        dbus::ObjectPath(kNetworkManagerPath));
    EXPECT_CALL(*mock_network_manager_proxy_, CallMethod(_, _, _))
        .WillRepeatedly(Invoke(
            this, &GeolocationWifiDataProviderLinuxTest::OnNetworkManagerCall));

    // Create mock proxies for the network devices.
    mock_device_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kNetworkManagerServiceName,
        dbus::ObjectPath(kDevicePath));
    EXPECT_CALL(*mock_device_proxy_, CallMethod(_, _, _))
        .WillRepeatedly(
            Invoke(this, &GeolocationWifiDataProviderLinuxTest::OnDeviceCall));
    mock_device_proxy2_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kNetworkManagerServiceName,
        dbus::ObjectPath(kDevicePath2));
    EXPECT_CALL(*mock_device_proxy2_, CallMethod(_, _, _))
        .WillRepeatedly(
            Invoke(this, &GeolocationWifiDataProviderLinuxTest::OnDeviceCall2));

    // Create mock proxies for the access points.
    mock_access_point_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kNetworkManagerServiceName,
        dbus::ObjectPath(kAccessPointPath));
    EXPECT_CALL(*mock_access_point_proxy_, CallMethod(_, _, _))
        .WillRepeatedly(Invoke(
            this, &GeolocationWifiDataProviderLinuxTest::OnAccessPointCall));
    mock_access_point_proxy2_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kNetworkManagerServiceName,
        dbus::ObjectPath(kAccessPointPath2));
    EXPECT_CALL(*mock_access_point_proxy2_, CallMethod(_, _, _))
        .WillRepeatedly(Invoke(
            this, &GeolocationWifiDataProviderLinuxTest::OnAccessPointCall2));
    mock_access_point_proxy3_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kNetworkManagerServiceName,
        dbus::ObjectPath(kAccessPointPath3));
    EXPECT_CALL(*mock_access_point_proxy3_, CallMethod(_, _, _))
        .WillRepeatedly(Invoke(
            this, &GeolocationWifiDataProviderLinuxTest::OnAccessPointCall3));

    // Set expectations for GetObjectProxy.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kNetworkManagerServiceName,
                               dbus::ObjectPath(kNetworkManagerPath)))
        .WillRepeatedly(Return(mock_network_manager_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(), GetObjectProxy(kNetworkManagerServiceName,
                                                 dbus::ObjectPath(kDevicePath)))
        .WillRepeatedly(Return(mock_device_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kNetworkManagerServiceName,
                               dbus::ObjectPath(kDevicePath2)))
        .WillRepeatedly(Return(mock_device_proxy2_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kNetworkManagerServiceName,
                               dbus::ObjectPath(kAccessPointPath)))
        .WillRepeatedly(Return(mock_access_point_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kNetworkManagerServiceName,
                               dbus::ObjectPath(kAccessPointPath2)))
        .WillRepeatedly(Return(mock_access_point_proxy2_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kNetworkManagerServiceName,
                               dbus::ObjectPath(kAccessPointPath3)))
        .WillRepeatedly(Return(mock_access_point_proxy3_.get()));

    // Create the wlan API with the mock bus object injected.
    wifi_provider_linux_ = base::MakeRefCounted<WifiDataProviderLinux>();
    wlan_api_ = wifi_provider_linux_->CreateWlanApiForTesting(mock_bus_);
    ASSERT_TRUE(wlan_api_);
  }

  void TearDown() override {
    wlan_api_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  void OnNetworkManagerCall(dbus::MethodCall* method_call,
                            int timeout_ms,
                            dbus::ObjectProxy::ResponseCallback callback) {
    if (method_call->GetInterface() == "org.freedesktop.NetworkManager" &&
        method_call->GetMember() == "GetDevices") {
      std::vector<dbus::ObjectPath> object_paths;
      object_paths.emplace_back(kDevicePath);

      std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      writer.AppendArrayOfObjectPaths(object_paths);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), base::Owned(response.release())));
      return;
    }

    FAIL() << "Unexpected method call: " << method_call->ToString();
  }

  void OnDeviceCall(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback callback) {
    std::unique_ptr<dbus::Response> response;
    if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
        method_call->GetMember() == "Get") {
      dbus::MessageReader reader(method_call);
      std::string interface_name;
      std::string property_name;
      if (reader.PopString(&interface_name) &&
          reader.PopString(&property_name) && property_name == "DeviceType") {
        response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        // This matches NM_DEVICE_TYPE_WIFI.
        const int kDeviceTypeWifi = 2;
        writer.AppendVariantOfUint32(kDeviceTypeWifi);
      }
    } else if (method_call->GetInterface() ==
                   "org.freedesktop.NetworkManager.Device.Wireless" &&
               method_call->GetMember() == "GetAccessPoints") {
      response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      std::vector<dbus::ObjectPath> object_paths;
      object_paths.emplace_back(kAccessPointPath);
      writer.AppendArrayOfObjectPaths(object_paths);
    }

    if (response) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), base::Owned(response.release())));
    } else {
      FAIL() << "Unexpected method call: " << method_call->ToString();
    }
  }

  void OnDeviceCall2(dbus::MethodCall* method_call,
                     int timeout_ms,
                     dbus::ObjectProxy::ResponseCallback callback) {
    std::unique_ptr<dbus::Response> response;
    if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
        method_call->GetMember() == "Get") {
      dbus::MessageReader reader(method_call);
      std::string interface_name;
      std::string property_name;
      if (reader.PopString(&interface_name) &&
          reader.PopString(&property_name) && property_name == "DeviceType") {
        response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        // This matches NM_DEVICE_TYPE_WIFI.
        const int kDeviceTypeWifi = 2;
        writer.AppendVariantOfUint32(kDeviceTypeWifi);
      }
    } else if (method_call->GetInterface() ==
                   "org.freedesktop.NetworkManager.Device.Wireless" &&
               method_call->GetMember() == "GetAccessPoints") {
      response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      std::vector<dbus::ObjectPath> object_paths;
      object_paths.emplace_back(kAccessPointPath2);
      object_paths.emplace_back(kAccessPointPath3);
      writer.AppendArrayOfObjectPaths(object_paths);
    }

    if (response) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), base::Owned(response.release())));
    } else {
      FAIL() << "Unexpected method call: " << method_call->ToString();
    }
  }

  void OnAccessPointCall(dbus::MethodCall* method_call,
                         int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
    OnAccessPointCallImpl(method_call, std::move(callback), "00-11-22-33-44-55",
                          100, 2427);
  }

  void OnAccessPointCall2(dbus::MethodCall* method_call,
                          int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback callback) {
    OnAccessPointCallImpl(method_call, std::move(callback), "00-11-22-33-44-56",
                          80, 2428);
  }

  void OnAccessPointCall3(dbus::MethodCall* method_call,
                          int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback callback) {
    OnAccessPointCallImpl(method_call, std::move(callback), "00-11-22-33-44-57",
                          60, 2429);
  }

  void OnAccessPointCallImpl(dbus::MethodCall* method_call,
                             dbus::ObjectProxy::ResponseCallback callback,
                             const std::string& mac_address,
                             uint8_t strength,
                             uint32_t frequency) {
    std::unique_ptr<dbus::Response> response;
    if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
        method_call->GetMember() == "Get") {
      dbus::MessageReader reader(method_call);
      std::string interface_name;
      std::string property_name;
      if (reader.PopString(&interface_name) &&
          reader.PopString(&property_name)) {
        response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());

        if (property_name == "HwAddress") {
          writer.AppendVariantOfString(mac_address);
        } else if (property_name == "Strength") {
          writer.AppendVariantOfByte(strength);
        } else if (property_name == "Frequency") {
          writer.AppendVariantOfUint32(frequency);
        }
      }
    }

    if (response) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), base::Owned(response.release())));
    } else {
      FAIL() << "Unexpected method call: " << method_call->ToString();
    }
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_network_manager_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_access_point_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_access_point_proxy2_;
  scoped_refptr<dbus::MockObjectProxy> mock_access_point_proxy3_;
  scoped_refptr<dbus::MockObjectProxy> mock_device_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_device_proxy2_;
  scoped_refptr<WifiDataProviderLinux> wifi_provider_linux_;
  std::unique_ptr<WifiDataProviderCommon::WlanApiInterface> wlan_api_;
};

TEST_F(GeolocationWifiDataProviderLinuxTest, GetAccessPointData) {
  base::test::TestFuture<std::unique_ptr<WifiData::AccessPointDataSet>> future;
  wlan_api_->GetAccessPointData(future.GetCallback());
  const auto& access_point_data_set = *future.Get();

  ASSERT_EQ(1u, access_point_data_set.size());
  const auto& access_point_data = *access_point_data_set.begin();

  // Check the contents of the access point data.
  // The expected values come from OnAccessPointCall() above.
  EXPECT_EQ("00-11-22-33-44-55", access_point_data.mac_address);
  EXPECT_EQ(-50, access_point_data.radio_signal_strength);
  EXPECT_EQ(4, access_point_data.channel);
}

TEST_F(GeolocationWifiDataProviderLinuxTest, GetAccessPointDataFailure) {
  EXPECT_CALL(*mock_network_manager_proxy_, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        std::move(callback).Run(nullptr);
      });

  base::test::TestFuture<std::unique_ptr<WifiData::AccessPointDataSet>> future;
  wlan_api_->GetAccessPointData(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(GeolocationWifiDataProviderLinuxTest, MultipleAdaptersAndAPs) {
  // Override the default OnNetworkManagerCall to return multiple devices.
  EXPECT_CALL(*mock_network_manager_proxy_, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetInterface() == "org.freedesktop.NetworkManager" &&
            method_call->GetMember() == "GetDevices") {
          std::vector<dbus::ObjectPath> object_paths;
          object_paths.emplace_back(kDevicePath);
          object_paths.emplace_back(kDevicePath2);

          std::unique_ptr<dbus::Response> response =
              dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendArrayOfObjectPaths(object_paths);
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback),
                                        base::Owned(response.release())));
          return;
        }
        FAIL() << "Unexpected method call: " << method_call->ToString();
      });

  base::test::TestFuture<std::unique_ptr<WifiData::AccessPointDataSet>> future;
  wlan_api_->GetAccessPointData(future.GetCallback());
  const auto& access_point_data_set = *future.Get();

  ASSERT_EQ(3u, access_point_data_set.size());
}

}  // namespace device
