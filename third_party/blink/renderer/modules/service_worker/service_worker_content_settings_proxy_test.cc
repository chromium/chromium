// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_content_settings_proxy.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

class MockWorkerContentSettingsProxy
    : public mojom::blink::WorkerContentSettingsProxy {
 public:
  MockWorkerContentSettingsProxy() = default;
  ~MockWorkerContentSettingsProxy() override = default;

  mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy> CreateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void set_allow_indexed_db(bool allow) { allow_indexed_db_ = allow; }
  void set_allow_cache_storage(bool allow) { allow_cache_storage_ = allow; }
  void set_allow_web_locks(bool allow) { allow_web_locks_ = allow; }

  // mojom::blink::WorkerContentSettingsProxy implementation:
  void AllowIndexedDB(AllowIndexedDBCallback callback) override {
    std::move(callback).Run(allow_indexed_db_);
  }
  void AllowCacheStorage(AllowCacheStorageCallback callback) override {
    std::move(callback).Run(allow_cache_storage_);
  }
  void AllowWebLocks(AllowWebLocksCallback callback) override {
    std::move(callback).Run(allow_web_locks_);
  }
  void RequestFileSystemAccessSync(
      RequestFileSystemAccessSyncCallback callback) override {
    std::move(callback).Run(false);
  }

 private:
  mojo::Receiver<mojom::blink::WorkerContentSettingsProxy> receiver_{this};
  bool allow_indexed_db_ = true;
  bool allow_cache_storage_ = true;
  bool allow_web_locks_ = true;
};

// Tests for ServiceWorkerContentSettingsProxy.
// The proxy calls ServiceWorkerContentSettingsProxy::GetService(), which stores
// a mojo::Remote in ThreadSpecific storage. Running tests on a dedicated worker
// thread ensures that ThreadSpecific state is cleanly destroyed when the thread
// stops after each test, preventing state leakage across multiple tests.
class ServiceWorkerContentSettingsProxyTest : public testing::Test {
 protected:
  void SetUp() override {
    worker_thread_ = NonMainThread::CreateThread(
        ThreadCreationParams(ThreadType::kTestThread)
            .SetThreadNameForTest("worker_thread"));
    mock_proxy_ = std::make_unique<MockWorkerContentSettingsProxy>();
    proxy_ = std::make_unique<ServiceWorkerContentSettingsProxy>(
        mock_proxy_->CreateRemote());
  }

  void TearDown() override {
    // Delete the proxy on the worker thread, then stop the worker thread to
    // ensure ThreadSpecific storage is cleaned up.
    worker_thread_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<ServiceWorkerContentSettingsProxy>) {},
            std::move(proxy_)));
    worker_thread_.reset();
    mock_proxy_.reset();
  }

  bool RunAllowStorageAccess(
      WebContentSettingsClient::StorageType storage_type) {
    bool result = false;
    base::RunLoop loop;
    worker_thread_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](ServiceWorkerContentSettingsProxy* proxy,
               WebContentSettingsClient::StorageType storage_type,
               base::OnceCallback<void(bool)> callback) {
              proxy->AllowStorageAccess(storage_type, std::move(callback));
            },
            proxy_.get(), storage_type,
            base::BindPostTask(
                base::SingleThreadTaskRunner::GetCurrentDefault(),
                base::BindOnce(
                    [](bool* out_result, base::OnceClosure quit_closure,
                       bool allow) {
                      *out_result = allow;
                      std::move(quit_closure).Run();
                    },
                    &result, loop.QuitClosure()))));
    loop.Run();
    return result;
  }

  bool RunAllowStorageAccessSync(
      WebContentSettingsClient::StorageType storage_type) {
    bool result = false;
    base::RunLoop loop;
    worker_thread_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](ServiceWorkerContentSettingsProxy* proxy,
                          WebContentSettingsClient::StorageType storage_type,
                          base::OnceCallback<void(bool)> reply_callback) {
                         bool allowed =
                             proxy->AllowStorageAccessSync(storage_type);
                         std::move(reply_callback).Run(allowed);
                       },
                       proxy_.get(), storage_type,
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           base::BindOnce(
                               [](bool* out_result,
                                  base::OnceClosure quit_closure, bool allow) {
                                 *out_result = allow;
                                 std::move(quit_closure).Run();
                               },
                               &result, loop.QuitClosure()))));
    loop.Run();
    return result;
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<NonMainThread> worker_thread_;
  std::unique_ptr<MockWorkerContentSettingsProxy> mock_proxy_;
  std::unique_ptr<ServiceWorkerContentSettingsProxy> proxy_;
};

TEST_F(ServiceWorkerContentSettingsProxyTest, AllowIndexedDB) {
  // Test IndexedDB async
  mock_proxy_->set_allow_indexed_db(false);
  EXPECT_FALSE(
      RunAllowStorageAccess(WebContentSettingsClient::StorageType::kIndexedDB));
  mock_proxy_->set_allow_indexed_db(true);
  EXPECT_TRUE(
      RunAllowStorageAccess(WebContentSettingsClient::StorageType::kIndexedDB));

  // Test IndexedDB sync
  mock_proxy_->set_allow_indexed_db(false);
  EXPECT_FALSE(RunAllowStorageAccessSync(
      WebContentSettingsClient::StorageType::kIndexedDB));
  mock_proxy_->set_allow_indexed_db(true);
  EXPECT_TRUE(RunAllowStorageAccessSync(
      WebContentSettingsClient::StorageType::kIndexedDB));
}

TEST_F(ServiceWorkerContentSettingsProxyTest, AllowCacheStorage) {
  // Test CacheStorage async
  mock_proxy_->set_allow_cache_storage(false);
  EXPECT_FALSE(RunAllowStorageAccess(
      WebContentSettingsClient::StorageType::kCacheStorage));
  mock_proxy_->set_allow_cache_storage(true);
  EXPECT_TRUE(RunAllowStorageAccess(
      WebContentSettingsClient::StorageType::kCacheStorage));

  // Test CacheStorage sync
  mock_proxy_->set_allow_cache_storage(false);
  EXPECT_FALSE(RunAllowStorageAccessSync(
      WebContentSettingsClient::StorageType::kCacheStorage));
  mock_proxy_->set_allow_cache_storage(true);
  EXPECT_TRUE(RunAllowStorageAccessSync(
      WebContentSettingsClient::StorageType::kCacheStorage));
}

TEST_F(ServiceWorkerContentSettingsProxyTest, AllowWebLocks) {
  // Test WebLocks async
  mock_proxy_->set_allow_web_locks(false);
  EXPECT_FALSE(
      RunAllowStorageAccess(WebContentSettingsClient::StorageType::kWebLocks));
  mock_proxy_->set_allow_web_locks(true);
  EXPECT_TRUE(
      RunAllowStorageAccess(WebContentSettingsClient::StorageType::kWebLocks));

  // Test WebLocks sync
  mock_proxy_->set_allow_web_locks(false);
  EXPECT_FALSE(RunAllowStorageAccessSync(
      WebContentSettingsClient::StorageType::kWebLocks));
  mock_proxy_->set_allow_web_locks(true);
  EXPECT_TRUE(RunAllowStorageAccessSync(
      WebContentSettingsClient::StorageType::kWebLocks));
}

}  // namespace
}  // namespace blink
