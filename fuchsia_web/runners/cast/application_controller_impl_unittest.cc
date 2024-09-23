// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromium/cast/cpp/fidl.h>
#include <fidl/chromium.cast/cpp/test_base.h>
#include <fuchsia/web/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl_test_base.h>
#include <lib/async/default.h>

#include <string>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "fuchsia_web/runners/cast/application_controller_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;

namespace {

class MockFrame final : public fuchsia::web::testing::Frame_TestBase {
 public:
  void NotImplemented_(const std::string& name) override {
    LOG(FATAL) << "No mock defined for " << name;
  }

  MOCK_METHOD(void,
              ConfigureInputTypes,
              (fuchsia::web::InputTypes types,
               fuchsia::web::AllowInputState allow));

  MOCK_METHOD(void,
              GetPrivateMemorySize,
              (GetPrivateMemorySizeCallback callback));
};

class ApplicationControllerImplTest
    : public fidl::testing::TestBase<chromium_cast::ApplicationContext>,
      public testing::Test {
 public:
  ApplicationControllerImplTest() {
    auto application_context_endpoints =
        fidl::CreateEndpoints<chromium_cast::ApplicationContext>();
    ZX_CHECK(application_context_endpoints.is_ok(),
             application_context_endpoints.status_value());
    application_context_binding_.emplace(
        async_get_default_dispatcher(),
        std::move(application_context_endpoints->server), this,
        [](fidl::UnbindInfo info) { ADD_FAILURE(); });
    application_context_.Bind(std::move(application_context_endpoints->client),
                              async_get_default_dispatcher());
    application_.emplace(&frame_, application_context_, /*trace_flow_id=*/0);
    base::RunLoop run_loop;
    wait_for_controller_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  ApplicationControllerImplTest(const ApplicationControllerImplTest&) = delete;
  ApplicationControllerImplTest& operator=(
      const ApplicationControllerImplTest&) = delete;

  ~ApplicationControllerImplTest() override = default;

 protected:
  void NotImplemented_(const std::string& name,
                       ::fidl::CompleterBase& completer) override {}

  // chromium_cast::ApplicationContext implementation.
  void GetMediaSessionId(GetMediaSessionIdCompleter::Sync& completer) override {
    NOTREACHED();
  }
  void SetApplicationController(
      SetApplicationControllerRequest& request,
      SetApplicationControllerCompleter::Sync& ignored_completer) override {
    EXPECT_TRUE(wait_for_controller_callback_);

    application_client_.Bind(std::move(request.controller()),
                             async_get_default_dispatcher());
    std::move(wait_for_controller_callback_).Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  MockFrame frame_;
  std::optional<fidl::ServerBinding<chromium_cast::ApplicationContext>>
      application_context_binding_;
  fidl::Client<chromium_cast::ApplicationContext> application_context_;
  std::optional<ApplicationControllerImpl> application_;

  fidl::Client<chromium_cast::ApplicationController> application_client_;
  base::OnceClosure wait_for_controller_callback_;
};

// Verifies that SetTouchInputEnabled() calls the Frame API correctly.
TEST_F(ApplicationControllerImplTest, ConfigureInputTypes) {
  base::RunLoop run_loop;

  EXPECT_CALL(frame_,
              ConfigureInputTypes(fuchsia::web::InputTypes::GESTURE_TAP |
                                      fuchsia::web::InputTypes::GESTURE_DRAG,
                                  fuchsia::web::AllowInputState::ALLOW))
      .Times(2);
  EXPECT_CALL(frame_,
              ConfigureInputTypes(fuchsia::web::InputTypes::GESTURE_TAP |
                                      fuchsia::web::InputTypes::GESTURE_DRAG,
                                  fuchsia::web::AllowInputState::DENY))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  EXPECT_TRUE(application_client_->SetTouchInputEnabled(true).is_ok());
  EXPECT_TRUE(application_client_->SetTouchInputEnabled(true).is_ok());
  EXPECT_TRUE(application_client_->SetTouchInputEnabled(false).is_ok());
  run_loop.Run();
}

// Verifies that SetTouchInputEnabled() calls the Frame API correctly.
TEST_F(ApplicationControllerImplTest, GetPrivateMemorySize) {
  constexpr uint64_t kMockSize = 12345;

  EXPECT_CALL(frame_, GetPrivateMemorySize(testing::_))
      .WillOnce(
          [](chromium::cast::ApplicationController::GetPrivateMemorySizeCallback
                 callback) { callback(kMockSize); });

  base::RunLoop loop;
  application_client_->GetPrivateMemorySize().Then(
      [quit_closure = loop.QuitClosure(),
       kMockSize](fidl::Result<
                  chromium_cast::ApplicationController::GetPrivateMemorySize>&
                      result) {
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result->size_bytes(), kMockSize);
        quit_closure.Run();
      });
  loop.Run();
}

}  // namespace
