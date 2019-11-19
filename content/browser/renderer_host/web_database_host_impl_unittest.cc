// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_database_host_impl.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/isolation_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fake_mojo_message_dispatch_context.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

base::string16 ConstructVfsFileName(const url::Origin& origin,
                                    const base::string16& name,
                                    const base::string16& suffix) {
  std::string identifier = storage::GetIdentifierFromOrigin(origin);
  return base::UTF8ToUTF16(identifier) + base::ASCIIToUTF16("/") + name +
         base::ASCIIToUTF16("#") + suffix;
}

}  // namespace

class WebDatabaseHostImplTest : public ::testing::Test {
 protected:
  WebDatabaseHostImplTest() = default;
  ~WebDatabaseHostImplTest() override = default;

  void SetUp() override {
    render_process_host_ =
        std::make_unique<MockRenderProcessHost>(&browser_context_);

    scoped_refptr<storage::DatabaseTracker> db_tracker =
        base::MakeRefCounted<storage::DatabaseTracker>(
            base::FilePath(), /*is_incognito=*/false,
            /*special_storage_policy=*/nullptr,
            /*quota_manager_proxy=*/nullptr);
    db_tracker->set_task_runner_for_testing(
        base::ThreadTaskRunnerHandle::Get());

    host_ = std::make_unique<WebDatabaseHostImpl>(process_id(),
                                                  std::move(db_tracker));
  }

  template <typename Callable>
  void CheckUnauthorizedOrigin(const Callable& func) {
    FakeMojoMessageDispatchContext fake_dispatch_context;
    mojo::test::BadMessageObserver bad_message_observer;
    func();
    EXPECT_EQ("Unauthorized origin.", bad_message_observer.WaitForBadMessage());
  }

  template <typename Callable>
  void CheckInvalidOrigin(const Callable& func) {
    FakeMojoMessageDispatchContext fake_dispatch_context;
    mojo::test::BadMessageObserver bad_message_observer;
    func();
    EXPECT_EQ("Invalid origin.", bad_message_observer.WaitForBadMessage());
  }

  void CallRenderProcessHostCleanup() {
    render_process_host_->Cleanup();

    // Releasing our handle on this object because Cleanup() posts a task
    // to delete the object and we need to avoid a double delete.
    render_process_host_.release();
  }

  WebDatabaseHostImpl* host() { return host_.get(); }
  int process_id() const { return render_process_host_->GetID(); }
  BrowserContext* browser_context() { return &browser_context_; }

 private:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  std::unique_ptr<WebDatabaseHostImpl> host_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseHostImplTest);
};

TEST_F(WebDatabaseHostImplTest, BadMessagesUnauthorized) {
  const url::Origin correct_origin =
      url::Origin::Create(GURL("http://correct.com"));
  const url::Origin incorrect_origin =
      url::Origin::Create(GURL("http://incorrect.net"));
  const base::string16 db_name(base::ASCIIToUTF16("db_name"));
  const base::string16 suffix(base::ASCIIToUTF16("suffix"));
  const base::string16 bad_vfs_file_name =
      ConstructVfsFileName(incorrect_origin, db_name, suffix);

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->AddIsolatedOrigins(
      {correct_origin, incorrect_origin},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  security_policy->LockToOrigin(IsolationContext(browser_context()),
                                process_id(), correct_origin.GetURL());
  ASSERT_TRUE(
      security_policy->CanAccessDataForOrigin(process_id(), correct_origin));
  ASSERT_FALSE(
      security_policy->CanAccessDataForOrigin(process_id(), incorrect_origin));

  CheckUnauthorizedOrigin([&]() {
    host()->OpenFile(bad_vfs_file_name,
                     /*desired_flags=*/0, base::BindOnce([](base::File) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->DeleteFile(bad_vfs_file_name,
                       /*sync_dir=*/false, base::BindOnce([](int32_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetFileAttributes(bad_vfs_file_name,
                              base::BindOnce([](int32_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetFileSize(bad_vfs_file_name, base::BindOnce([](int64_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->SetFileSize(bad_vfs_file_name, /*expected_size=*/0,
                        base::BindOnce([](bool) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetSpaceAvailable(incorrect_origin, base::BindOnce([](int64_t) {}));
  });

  CheckUnauthorizedOrigin([&]() {
    host()->Opened(incorrect_origin, db_name, base::ASCIIToUTF16("description"),
                   /*estimated_size=*/0);
  });

  CheckUnauthorizedOrigin(
      [&]() { host()->Modified(incorrect_origin, db_name); });

  CheckUnauthorizedOrigin([&]() { host()->Closed(incorrect_origin, db_name); });

  CheckUnauthorizedOrigin([&]() {
    host()->HandleSqliteError(incorrect_origin, db_name, /*error=*/0);
  });
}

TEST_F(WebDatabaseHostImplTest, BadMessagesInvalid) {
  const url::Origin opaque_origin;
  const base::string16 db_name(base::ASCIIToUTF16("db_name"));

  CheckInvalidOrigin([&]() {
    host()->GetSpaceAvailable(opaque_origin, base::BindOnce([](int64_t) {}));
  });

  CheckInvalidOrigin([&]() {
    host()->Opened(opaque_origin, db_name, base::ASCIIToUTF16("description"),
                   /*estimated_size=*/0);
  });

  CheckInvalidOrigin([&]() { host()->Modified(opaque_origin, db_name); });

  CheckInvalidOrigin([&]() { host()->Closed(opaque_origin, db_name); });

  CheckInvalidOrigin([&]() {
    host()->HandleSqliteError(opaque_origin, db_name, /*error=*/0);
  });
}

TEST_F(WebDatabaseHostImplTest, ProcessShutdown) {
  const url::Origin correct_origin =
      url::Origin::Create(GURL("http://correct.com"));
  const url::Origin incorrect_origin =
      url::Origin::Create(GURL("http://incorrect.net"));
  const base::string16 db_name(base::ASCIIToUTF16("db_name"));
  const base::string16 suffix(base::ASCIIToUTF16("suffix"));
  const base::string16 bad_vfs_file_name =
      ConstructVfsFileName(incorrect_origin, db_name, suffix);

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->AddIsolatedOrigins(
      {correct_origin, incorrect_origin},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  security_policy->LockToOrigin(IsolationContext(browser_context()),
                                process_id(), correct_origin.GetURL());

  bool success_callback_was_called = false;
  auto success_callback = base::BindLambdaForTesting(
      [&](base::File) { success_callback_was_called = true; });
  base::Optional<std::string> error_callback_message;

  mojo::core::SetDefaultProcessErrorCallback(base::BindLambdaForTesting(
      [&](const std::string& message) { error_callback_message = message; }));

  // Verify that an error occurs with OpenFile() call before process shutdown.
  {
    FakeMojoMessageDispatchContext fake_dispatch_context;
    host()->OpenFile(bad_vfs_file_name,
                     /*desired_flags=*/0, success_callback);

    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(success_callback_was_called);
    EXPECT_TRUE(error_callback_message.has_value());
    EXPECT_EQ("Unauthorized origin.", error_callback_message.value());
  }

  success_callback_was_called = false;
  error_callback_message.reset();

  // Start cleanup of the RenderProcessHost. This causes
  // RenderProcessHost::FromID() to return nullptr for the process_id.
  CallRenderProcessHostCleanup();

  // Attempt the call again and verify that no callbacks were called.
  {
    FakeMojoMessageDispatchContext fake_dispatch_context;
    host()->OpenFile(bad_vfs_file_name,
                     /*desired_flags=*/0, success_callback);

    base::RunLoop().RunUntilIdle();

    // Verify none of the callbacks were called.
    EXPECT_FALSE(success_callback_was_called);
    EXPECT_FALSE(error_callback_message.has_value());
  }

  mojo::core::SetDefaultProcessErrorCallback(
      mojo::core::ProcessErrorCallback());
}

}  // namespace content
