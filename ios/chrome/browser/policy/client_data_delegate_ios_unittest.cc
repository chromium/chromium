// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/client_data_delegate_ios.h"

#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(ClientDataDelegateIosTest, FillRegisterBrowserRequest) {
  ClientDataDelegateIos client_data_delegate;
  enterprise_management::RegisterBrowserRequest request;
  client_data_delegate.FillRegisterBrowserRequest(&request);

  EXPECT_EQ(request.device_model(), GetDeviceModel());
  EXPECT_EQ(request.brand_name(), GetDeviceManufacturer());

  // Fields that shouldn't be filled on iOS due to Privacy concerns.
  EXPECT_TRUE(request.machine_name().empty());
  EXPECT_TRUE(request.browser_device_identifier().computer_name().empty());
  EXPECT_TRUE(request.browser_device_identifier().serial_number().empty());
}

}  // namespace policy
