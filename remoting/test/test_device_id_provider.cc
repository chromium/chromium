// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_device_id_provider.h"

#include <utility>

#include "base/logging.h"
#include "base/uuid.h"
#include "build/build_config.h"

namespace remoting {
namespace test {

TestDeviceIdProvider::TestDeviceIdProvider(TokenStorage* token_storage)
    : token_storage_(token_storage) {
  DCHECK(token_storage_);
}

TestDeviceIdProvider::~TestDeviceIdProvider() = default;

ftl::DeviceId TestDeviceIdProvider::GetDeviceId() {
  std::string id = token_storage_->FetchDeviceId();
  if (id.empty()) {
    id = "crd-test-" + base::Uuid::GenerateRandomV4().AsLowercaseString();
    VLOG(0) << "Generated new device_id: " << id;
    token_storage_->StoreDeviceId(id);
  } else {
    VLOG(0) << "Using stored device_id: " << id;
  }
  ftl::DeviceId device_id;
  device_id.set_type(ftl::DeviceIdType_Type_WEB_UUID);
  device_id.set_id(id);
  return device_id;
}

}  // namespace test
}  // namespace remoting
