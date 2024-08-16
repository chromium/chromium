// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_database/web_database_host_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/isolation_context.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

std::u16string ConstructVfsFileName(const url::Origin& origin,
                                    const std::u16string& name,
                                    const std::u16string& suffix) {
  std::string identifier = storage::GetIdentifierFromOrigin(origin);
  return base::UTF8ToUTF16(identifier) + u"/" + name + u"#" + suffix;
}

class WebDatabaseHostImplTest : public ::testing::Test {
 public:
  WebDatabaseHostImplTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {}
  WebDatabaseHostImplTest(const WebDatabaseHostImplTest&) = delete;
  WebDatabaseHostImplTest& operator=(const WebDatabaseHostImplTest&) = delete;
  ~WebDatabaseHostImplTest() override = default;

  void SetUp() override {
    render_process_host_ =
        std::make_unique<MockRenderProcessHost>(&browser_context_);

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    db_tracker_ = storage::DatabaseTracker::Create(
        data_dir_.GetPath(), /*is_incognito=*/false, quota_manager_proxy_);
    // Raw pointer usage is safe because `host_` stores a reference to the
    // DatabaseTracker, keeping it alive for the duration of the test.
    task_runner_ = db_tracker_->task_runner();
    host_ = std::make_unique<WebDatabaseHostImpl>(process_id(), db_tracker_);
  }

  void TearDown() override {
    base::RunLoop run_loop;
    task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                             db_tracker_->Shutdown();
                             run_loop.Quit();
                           }));
    run_loop.Run();
    task_runner_->DeleteSoon(FROM_HERE, std::move(host_));
    RunUntilIdle();
  }

 protected:
  template <typename Callable>
  void CheckUnauthorizedOrigin(const Callable& func) {
    mojo::test::BadMessageObserver bad_message_observer;
    base::RunLoop run_loop;
    task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          mojo::FakeMessageDispatchContext fake_dispatch_context;
          func();
          run_loop.Quit();
        }));
    run_loop.Run();
    RunUntilIdle();
    EXPECT_EQ("WebDatabaseHost: Unauthorized origin.",
              bad_message_observer.WaitForBadMessage());
  }

  template <typename Callable>
  void CheckInvalidOrigin(const Callable& func) {
    mojo::test::BadMessageObserver bad_message_observer;
    base::RunLoop run_loop;
    task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          mojo::FakeMessageDispatchContext fake_dispatch_context;
          func();
          run_loop.Quit();
        }));
    run_loop.Run();
    RunUntilIdle();
    EXPECT_EQ("WebDatabaseHost: Invalid origin.",
              bad_message_observer.WaitForBadMessage());
  }

  void CallRenderProcessHostCleanup() { render_process_host_.reset(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  WebDatabaseHostImpl* host() { return host_.get(); }
  int process_id() const { return render_process_host_->GetID(); }
  BrowserContext* browser_context() { return &browser_context_; }
  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

  void LockProcessToURL(const GURL& url) {
    ChildProcessSecurityPolicyImpl::GetInstance()->LockProcessForTesting(
        IsolationContext(
            BrowsingInstanceId(1), browser_context(),
            /*is_guest=*/false, /*is_fenced=*/false,
            OriginAgentClusterIsolationState::CreateForDefaultIsolation(
                &browser_context_)),
        process_id(), url);
  }

  storage::MockQuotaManager* quota_manager() { return quota_manager_.get(); }

  storage::QuotaManagerProxy* quota_manager_proxy() {
    return quota_manager_proxy_.get();
  }

 private:
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;
  BrowserTaskEnvironment task_environment_;

  TestBrowserContext browser_context_;
  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  scoped_refptr<storage::DatabaseTracker> db_tracker_;
  std::unique_ptr<WebDatabaseHostImpl> host_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
};

TEST_F(WebDatabaseHostImplTest, OpenFileCreatesBucket) {
  const char* example_url = "http://example.com";
  const GURL example_gurl(example_url);
  const url::Origin example_origin = url::Origin::Create(example_gurl);
  const std::u16string db_name = u"db_name";
  const std::u16string suffix(u"suffix");
  const std::u16string vfs_file_name =
      ConstructVfsFileName(example_origin, db_name, suffix);

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->AddFutureIsolatedOrigins(
      {example_origin}, ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  LockProcessToURL(example_gurl);

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy());

  base::RunLoop run_loop;
  task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        mojo::FakeMessageDispatchContext fake_dispatch_context;
        host()->OpenFile(vfs_file_name, /*desired_flags=*/0,
                         base::BindLambdaForTesting(
                             [&](base::File file) { run_loop.Quit(); }));
      }));
  run_loop.Run();

  // Check default bucket exists for https://example.com.
  ASSERT_OK_AND_ASSIGN(
      storage::BucketInfo result,
      quota_manager_proxy_sync.GetBucket(
          blink::StorageKey::CreateFromStringForTesting(example_url),
          storage::kDefaultBucketName, blink::mojom::StorageType::kTemporary));
  EXPECT_EQ(result.name, storage::kDefaultBucketName);
  EXPECT_EQ(result.storage_key,
            blink::StorageKey::CreateFromStringForTesting(example_url));
  EXPECT_GT(result.id.value(), 0);

  security_policy->ClearIsolatedOriginsForTesting();
}

TEST_F(WebDatabaseHostImplTest, GetOrCreateBucketError) {
  const char* example_url = "http://example.com";
  const GURL example_gurl(example_url);
  const url::Origin example_origin = url::Origin::Create(example_gurl);
  const std::u16string db_name = u"db_name";
  const std::u16string suffix(u"suffix");
  const std::u16string vfs_file_name =
      ConstructVfsFileName(example_origin, db_name, suffix);

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->AddFutureIsolatedOrigins(
      {example_origin}, ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  LockProcessToURL(example_gurl);

  quota_manager()->SetDisableDatabase(true);
  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy());

  base::RunLoop run_loop;
  task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        mojo::FakeMessageDispatchContext fake_dispatch_context;
        host()->OpenFile(vfs_file_name, /*desired_flags=*/0,
                         base::BindLambdaForTesting([&](base::File file) {
                           EXPECT_FALSE(file.IsValid());
                           run_loop.Quit();
                         }));
      }));
  run_loop.Run();

  security_policy->ClearIsolatedOriginsForTesting();
}

TEST_F(WebDatabaseHostImplTest, BadMessagesUnauthorized) {
  const GURL correct_url("http://correct.com");
  const url::Origin correct_origin = url::Origin::Create(correct_url);
  const url::Origin incorrect_origin =
      url::Origin::Create(GURL("http://incorrect.net"));
  const std::u16string db_name(u"db_name");
  const std::u16string suffix(u"suffix");
  const std::u16string bad_vfs_file_name =
      ConstructVfsFileName(incorrect_origin, db_name, suffix);

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->AddFutureIsolatedOrigins(
      {correct_origin, incorrect_origin},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  LockProcessToURL(correct_url);

  ASSERT_TRUE(
      security_policy->CanAccessDataForOrigin(process_id(), correct_origin));
  ASSERT_FALSE(
      security_policy->CanAccessDataForOrigin(process_id(), incorrect_origin));

  CheckUnauthorizedOrigin([&]() {
    host()->OpenFile(bad_vfs_file_name,
                     /*desired_flags=*/0, base::DoNothing());
  });

  CheckUnauthorizedOrigin([&]() {
    host()->DeleteFile(bad_vfs_file_name,
                       /*sync_dir=*/false, base::DoNothing());
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetFileAttributes(bad_vfs_file_name, base::DoNothing());
  });

  CheckUnauthorizedOrigin([&]() {
    host()->GetSpaceAvailable(incorrect_origin, base::DoNothing());
  });

  CheckUnauthorizedOrigin(
      [&]() { host()->Opened(incorrect_origin, db_name, u"description"); });

  CheckUnauthorizedOrigin(
      [&]() { host()->Modified(incorrect_origin, db_name); });

  CheckUnauthorizedOrigin([&]() { host()->Closed(incorrect_origin, db_name); });

  CheckUnauthorizedOrigin([&]() {
    host()->HandleSqliteError(incorrect_origin, db_name, /*error=*/0);
  });

  security_policy->ClearIsolatedOriginsForTesting();
}

TEST_F(WebDatabaseHostImplTest, BadMessagesInvalid) {
  const url::Origin opaque_origin;
  const std::u16string db_name(u"db_name");

  CheckInvalidOrigin(
      [&]() { host()->GetSpaceAvailable(opaque_origin, base::DoNothing()); });

  CheckInvalidOrigin(
      [&]() { host()->Opened(opaque_origin, db_name, u"description"); });

  CheckInvalidOrigin([&]() { host()->Modified(opaque_origin, db_name); });

  CheckInvalidOrigin([&]() { host()->Closed(opaque_origin, db_name); });

  CheckInvalidOrigin([&]() {
    host()->HandleSqliteError(opaque_origin, db_name, /*error=*/0);
  });
}

TEST_F(WebDatabaseHostImplTest, ProcessShutdown) {
  const GURL correct_url("http://correct.com");
  const url::Origin correct_origin = url::Origin::Create(correct_url);
  const url::Origin incorrect_origin =
      url::Origin::Create(GURL("http://incorrect.net"));
  const std::u16string db_name(u"db_name");
  const std::u16string suffix(u"suffix");
  const std::u16string bad_vfs_file_name =
      ConstructVfsFileName(incorrect_origin, db_name, suffix);

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->AddFutureIsolatedOrigins(
      {correct_origin, incorrect_origin},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  LockProcessToURL(correct_url);

  bool success_callback_was_called = false;
  auto success_callback = base::BindLambdaForTesting(
      [&](base::File) { success_callback_was_called = true; });
  std::optional<std::string> error_callback_message;

  mojo::SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& message) { error_callback_message = message; }));

  // Verify that an error occurs with OpenFile() call before process shutdown.
  {
    base::RunLoop run_loop;
    task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          mojo::FakeMessageDispatchContext fake_dispatch_context;
          host()->OpenFile(bad_vfs_file_name,
                           /*desired_flags=*/0, success_callback);
          run_loop.Quit();
        }));
    run_loop.Run();
    RunUntilIdle();

    EXPECT_FALSE(success_callback_was_called);
    EXPECT_TRUE(error_callback_message.has_value());
    EXPECT_EQ("WebDatabaseHost: Unauthorized origin.",
              error_callback_message.value());
  }

  success_callback_was_called = false;
  error_callback_message.reset();

  // Start cleanup of the RenderProcessHost. This causes
  // RenderProcessHost::FromID() to return nullptr for the process_id.
  CallRenderProcessHostCleanup();

  // Attempt the call again and verify that no callbacks were called.
  {
    base::RunLoop run_loop;
    task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          mojo::FakeMessageDispatchContext fake_dispatch_context;
          host()->OpenFile(bad_vfs_file_name,
                           /*desired_flags=*/0, success_callback);
          run_loop.Quit();
        }));
    run_loop.Run();
    RunUntilIdle();

    // Verify none of the callbacks were called.
    EXPECT_FALSE(success_callback_was_called);
    EXPECT_FALSE(error_callback_message.has_value());
  }

  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  security_policy->ClearIsolatedOriginsForTesting();
}

}  // namespace

}  // namespace content
