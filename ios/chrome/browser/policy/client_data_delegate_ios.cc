// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/client_data_delegate_ios.h"

#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

void ClientDataDelegateIos::FillRegisterBrowserRequest(
    enterprise_management::RegisterBrowserRequest* request) const {
  request->set_os_platform(GetOSPlatform());
  request->set_os_version(GetOSVersion());
  request->set_device_model(GetDeviceModel());
  request->set_brand_name(GetDeviceManufacturer());
}

}  // namespace policy
