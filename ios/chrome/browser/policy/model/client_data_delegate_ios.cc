// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/client_data_delegate_ios.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/system/sys_info.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

void SetHardwareInfo(enterprise_management::RegisterBrowserRequest* request,
                     base::OnceClosure callback,
                     base::SysInfo::HardwareInfo hardware_info) {
  request->set_device_model(hardware_info.model);
  request->set_brand_name(hardware_info.manufacturer);
  std::move(callback).Run();
}

}  // namespace

void ClientDataDelegateIos::FillRegisterBrowserRequest(
    enterprise_management::RegisterBrowserRequest* request,
    base::OnceClosure callback) const {
  request->set_os_platform(GetOSPlatform());
  request->set_os_version(GetOSVersion());

  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&SetHardwareInfo, request, std::move(callback)));
}

}  // namespace policy
