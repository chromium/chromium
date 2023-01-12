// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/device/hid/input_service_linux.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {
void OnGetDevices(base::OnceClosure quit_closure,
                  std::vector<mojom::InputDeviceInfoPtr> devices) {
  for (size_t i = 0; i < devices.size(); ++i)
    ASSERT_TRUE(!devices[i]->id.empty());

  std::move(quit_closure).Run();
}
}  // namespace

TEST(InputServiceLinux, Simple) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  InputServiceLinux* service = InputServiceLinux::GetInstance();
  ASSERT_TRUE(service);
  base::RunLoop run_loop;
  service->GetDevices(base::BindOnce(&OnGetDevices, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace device
