// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/application_controller_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;

namespace {

class MockFrame : public fuchsia::web::testing::Frame_TestBase {
 public:
  void NotImplemented_(const std::string& name) final {
    LOG(FATAL) << "No mock defined for " << name;
  }

  MOCK_METHOD1(SetEnableInput, void(bool));
};

class ApplicationControllerImplTest
    : public chromium::cast::ApplicationControllerReceiver,
      public testing::Test {
 public:
  ApplicationControllerImplTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)),
        application_receiver_binding_(this),
        application_(&frame_, application_receiver_binding_.NewBinding()) {
    base::RunLoop run_loop;
    wait_for_controller_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  ~ApplicationControllerImplTest() override = default;

 protected:
  // chromium::cast::ApplicationReceiver implementation.
  void SetApplicationController(
      fidl::InterfaceHandle<chromium::cast::ApplicationController> application)
      final {
    DCHECK(wait_for_controller_callback_);

    application_ptr_ = application.Bind();
    std::move(wait_for_controller_callback_).Run();
  }

  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  MockFrame frame_;
  fidl::Binding<chromium::cast::ApplicationControllerReceiver>
      application_receiver_binding_;

  chromium::cast::ApplicationControllerPtr application_ptr_;
  ApplicationControllerImpl application_;
  base::OnceClosure wait_for_controller_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplicationControllerImplTest);
};

// Verifies that SetTouchInputEnabled() calls the Frame API correctly.
TEST_F(ApplicationControllerImplTest, SetEnableInput) {
  base::RunLoop run_loop;

  EXPECT_CALL(frame_, SetEnableInput(true)).Times(2);
  EXPECT_CALL(frame_, SetEnableInput(false))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  application_ptr_->SetTouchInputEnabled(true);
  application_ptr_->SetTouchInputEnabled(true);
  application_ptr_->SetTouchInputEnabled(false);
  run_loop.Run();
}

}  // namespace
