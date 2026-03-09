// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/uuid.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"
#include "remoting/signaling/ftl_host_device_id_provider.h"
#include "remoting/signaling/ftl_support_host_device_id_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(FtlDeviceIdProviderTest, ClientUuidDeviceIdProvider) {
  FtlClientUuidDeviceIdProvider provider;
  ftl::DeviceId device_id = provider.GetDeviceId();
  EXPECT_EQ(device_id.type(), ftl::DeviceIdType_Type_CLIENT_UUID);
  EXPECT_TRUE(base::Uuid::ParseCaseInsensitive(device_id.id()).is_valid());
}

TEST(FtlDeviceIdProviderTest, HostDeviceIdProvider) {
  FtlHostDeviceIdProvider provider("test_host_id");
  ftl::DeviceId device_id = provider.GetDeviceId();
  EXPECT_EQ(device_id.type(), ftl::DeviceIdType_Type_CHROMOTING_HOST_ID);
  EXPECT_TRUE(device_id.id().starts_with("crd-"));
  // The middle portion of the ID is platform specific.
  EXPECT_TRUE(device_id.id().ends_with("-test_host_id"));
}

TEST(FtlDeviceIdProviderTest, SupportHostDeviceIdProvider) {
  FtlSupportHostDeviceIdProvider provider("ftl-device-registration-id");
  ftl::DeviceId device_id = provider.GetDeviceId();
  EXPECT_EQ(device_id.type(), ftl::DeviceIdType_Type_CHROMOTING_HOST_ID);
  EXPECT_EQ(device_id.id(), "crd-it2me-host-ftl-device-registration-id");
}

}  // namespace remoting
