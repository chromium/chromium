// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/scoped_surface_request_manager.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "gpu/command_buffer/service/mock_texture_owner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

namespace content {

class ScopedSurfaceRequestManagerUnitTest : public testing::Test {
 public:
  ScopedSurfaceRequestManagerUnitTest() {
    manager_ = ScopedSurfaceRequestManager::GetInstance();

    // The need to reset the callbacks because the
    // ScopedSurfaceRequestManager's lifetime outlive the tests.
    manager_->clear_requests_for_testing();

    last_received_request_ = 0;
    dummy_token_ = base::UnguessableToken::CreateForTesting(123, 456);

    mock_texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
        0, nullptr, nullptr);
  }

  ScopedSurfaceRequestManagerUnitTest(
      const ScopedSurfaceRequestManagerUnitTest&) = delete;
  ScopedSurfaceRequestManagerUnitTest& operator=(
      const ScopedSurfaceRequestManagerUnitTest&) = delete;

  ScopedSurfaceRequestManager::ScopedSurfaceRequestCB CreateNoopCallback() {
    return base::BindOnce(&ScopedSurfaceRequestManagerUnitTest::DummyCallback,
                          base::Unretained(this));
  }

  ScopedSurfaceRequestManager::ScopedSurfaceRequestCB CreateLoggingCallback() {
    return base::BindOnce(&ScopedSurfaceRequestManagerUnitTest::LoggingCallback,
                          base::Unretained(this), kSpecificCallbackId);
  }

  // No-op callback.
  void DummyCallback(gl::ScopedJavaSurface surface) {}

  // Callback that updates |last_received_request_| to allow differentiation
  // between callback instances in tests.
  void LoggingCallback(int request_id, gl::ScopedJavaSurface surface) {
    last_received_request_ = request_id;
  }

  scoped_refptr<NiceMock<gpu::MockTextureOwner>> mock_texture_owner;

  int last_received_request_;
  const int kSpecificCallbackId = 1357;
  base::UnguessableToken dummy_token_;

  raw_ptr<ScopedSurfaceRequestManager> manager_;

  content::BrowserTaskEnvironment task_environment_;
};

// Makes sure we can successfully register a callback.
TEST_F(ScopedSurfaceRequestManagerUnitTest, RegisterRequest_ShouldSucceed) {
  EXPECT_EQ(0, manager_->request_count_for_testing());

  base::UnguessableToken token =
      manager_->RegisterScopedSurfaceRequest(CreateNoopCallback());

  EXPECT_EQ(1, manager_->request_count_for_testing());
  EXPECT_FALSE(token.is_empty());
}

// Makes sure we can successfully register multiple callbacks, and that they
// return distinct request tokens.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       RegisterMultipleRequests_ShouldSucceed) {
  base::UnguessableToken token1 =
      manager_->RegisterScopedSurfaceRequest(CreateNoopCallback());
  base::UnguessableToken token2 =
      manager_->RegisterScopedSurfaceRequest(CreateNoopCallback());

  EXPECT_EQ(2, manager_->request_count_for_testing());
  EXPECT_NE(token1, token2);
}

// Makes sure GetInstance() is idempotent/that the class is a proper singleton.
TEST_F(ScopedSurfaceRequestManagerUnitTest, VerifySingleton_ShouldSucceed) {
  EXPECT_EQ(manager_, ScopedSurfaceRequestManager::GetInstance());
}

// Makes sure we can unregister a callback after registering it.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       GetRegisteredRequest_ShouldSucceed) {
  base::UnguessableToken token =
      manager_->RegisterScopedSurfaceRequest(CreateNoopCallback());
  EXPECT_EQ(1, manager_->request_count_for_testing());

  manager_->UnregisterScopedSurfaceRequest(token);

  EXPECT_EQ(0, manager_->request_count_for_testing());
}

// Makes sure that unregistering a callback only affects the specified callback.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       GetRegisteredRequestFromMultipleRequests_ShouldSucceed) {
  base::UnguessableToken token =
      manager_->RegisterScopedSurfaceRequest(CreateNoopCallback());
  manager_->RegisterScopedSurfaceRequest(CreateNoopCallback());
  EXPECT_EQ(2, manager_->request_count_for_testing());

  manager_->UnregisterScopedSurfaceRequest(token);

  EXPECT_EQ(1, manager_->request_count_for_testing());
}

// Makes sure that unregistration is a noop permitted when there are no
// registered requests.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       UnregisteredRequest_ShouldReturnNullCallback) {
  manager_->UnregisterScopedSurfaceRequest(dummy_token_);

  EXPECT_EQ(0, manager_->request_count_for_testing());
}

// Makes sure that unregistering an invalid |request_token| doesn't affect
// other registered callbacks.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       GetUnregisteredRequestFromMultipleRequests_ShouldReturnNullCallback) {
  manager_->RegisterScopedSurfaceRequest(CreateNoopCallback());

  manager_->UnregisterScopedSurfaceRequest(dummy_token_);

  EXPECT_EQ(1, manager_->request_count_for_testing());
}

// Makes sure that trying to fulfill a request for an invalid |request_token|
// does nothing, and does not affect other callbacks.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       FulfillUnregisteredRequest_ShouldDoNothing) {
  manager_->RegisterScopedSurfaceRequest(CreateLoggingCallback());

  manager_->FulfillScopedSurfaceRequest(
      dummy_token_, mock_texture_owner->CreateJavaSurface());

  EXPECT_EQ(1, manager_->request_count_for_testing());
  EXPECT_NE(kSpecificCallbackId, last_received_request_);
}

// Makes sure that trying to fulfill a request fulfills the right request, and
// does not affect other registered requests.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       FulfillRegisteredRequest_ShouldSucceed) {
  base::UnguessableToken specific_token =
      manager_->RegisterScopedSurfaceRequest(CreateLoggingCallback());

  const uint64_t kOtherCallbackId = 5678;
  manager_->RegisterScopedSurfaceRequest(
      base::BindOnce(&ScopedSurfaceRequestManagerUnitTest::LoggingCallback,
                     base::Unretained(this), kOtherCallbackId));

  manager_->FulfillScopedSurfaceRequest(
      specific_token, mock_texture_owner->CreateJavaSurface());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, manager_->request_count_for_testing());
  EXPECT_EQ(kSpecificCallbackId, last_received_request_);
}

// Makes sure that the ScopedSurfaceRequestConduit implementation properly
// fulfills requests.
TEST_F(ScopedSurfaceRequestManagerUnitTest,
       ForwardSurfaceOwner_ShouldFulfillRequest) {
  base::UnguessableToken token =
      manager_->RegisterScopedSurfaceRequest(CreateLoggingCallback());

  manager_->ForwardSurfaceOwnerForSurfaceRequest(token,
                                                 mock_texture_owner.get());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, manager_->request_count_for_testing());
  EXPECT_EQ(kSpecificCallbackId, last_received_request_);
}

}  // Content
