// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class HidServiceTest : public ::testing::Test {
 public:
  HidServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<HidService> service_;
};

void OnGetDevices(base::OnceClosure quit_closure,
                  std::vector<mojom::HidDeviceInfoPtr> devices) {
  // Since there's no guarantee that any devices are connected at the moment
  // this test doesn't assume anything about the result but it at least verifies
  // that devices can be enumerated without the application crashing.
  std::move(quit_closure).Run();
}

}  // namespace

TEST_F(HidServiceTest, GetDevices) {
  service_ = HidService::Create();
  ASSERT_TRUE(service_);

  base::RunLoop loop;
  service_->GetDevices(base::BindOnce(&OnGetDevices, loop.QuitClosure()));
  loop.Run();
}

}  // namespace device
