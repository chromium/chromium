// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_database.h"
#include "content/browser/appcache/appcache_storage_impl.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/appcache_test_helper.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <set>

namespace content {
namespace {
const base::FilePath::CharType kTestingAppCacheDirname[] =
    FILE_PATH_LITERAL("Application Cache");

// Examples of a protected and an unprotected origin, to be used througout the
// test.
const char kProtectedManifest[] = "http://www.protected.com/cache.manifest";
const char kNormalManifest[] = "http://www.normal.com/cache.manifest";
const char kSessionOnlyManifest[] = "http://www.sessiononly.com/cache.manifest";

class MockURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  MockURLRequestContextGetter(
      net::URLRequestContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : context_(context), task_runner_(std::move(task_runner)) {}

  net::URLRequestContext* GetURLRequestContext() override { return context_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return task_runner_;
  }

 protected:
  ~MockURLRequestContextGetter() override {}

 private:
  net::URLRequestContext* context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

class ChromeAppCacheServiceTest : public testing::Test {
 public:
  ChromeAppCacheServiceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        kProtectedManifestURL(kProtectedManifest),
        kNormalManifestURL(kNormalManifest),
        kSessionOnlyManifestURL(kSessionOnlyManifest),
        thread_bundle_(TestBrowserThreadBundle::Options::IO_MAINLOOP) {}

 protected:
  scoped_refptr<ChromeAppCacheService> CreateAppCacheServiceImpl(
      const base::FilePath& appcache_path,
      bool init_storage);
  void InsertDataIntoAppCache(ChromeAppCacheService* appcache_service);

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir temp_dir_;
  const GURL kProtectedManifestURL;
  const GURL kNormalManifestURL;
  const GURL kSessionOnlyManifestURL;

 private:
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;
};

scoped_refptr<ChromeAppCacheService>
ChromeAppCacheServiceTest::CreateAppCacheServiceImpl(
    const base::FilePath& appcache_path,
    bool init_storage) {
  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService(nullptr);
  scoped_refptr<MockSpecialStoragePolicy> mock_policy =
      new MockSpecialStoragePolicy;
  mock_policy->AddProtected(kProtectedManifestURL.GetOrigin());
  mock_policy->AddSessionOnly(kSessionOnlyManifestURL.GetOrigin());
  scoped_refptr<MockURLRequestContextGetter> mock_request_context_getter =
      new MockURLRequestContextGetter(
          browser_context_.GetResourceContext()->GetRequestContext(),
          base::ThreadTaskRunnerHandle::Get());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &ChromeAppCacheService::InitializeOnIOThread, appcache_service,
          appcache_path, browser_context_.GetResourceContext(),
          base::RetainedRef(mock_request_context_getter), mock_policy));
  // Steps needed to initialize the storage of AppCache data.
  scoped_task_environment_.RunUntilIdle();
  if (init_storage) {
    AppCacheStorageImpl* storage =
        static_cast<AppCacheStorageImpl*>(
            appcache_service->storage());
    storage->database_->db_connection();
    storage->disk_cache();
    scoped_task_environment_.RunUntilIdle();
  }
  return appcache_service;
}

void ChromeAppCacheServiceTest::InsertDataIntoAppCache(
    ChromeAppCacheService* appcache_service) {
  AppCacheTestHelper appcache_helper;
  appcache_helper.AddGroupAndCache(appcache_service, kNormalManifestURL);
  appcache_helper.AddGroupAndCache(appcache_service, kProtectedManifestURL);
  appcache_helper.AddGroupAndCache(appcache_service, kSessionOnlyManifestURL);

  // Verify that adding the data succeeded
  std::set<url::Origin> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service, &origins);
  ASSERT_EQ(3UL, origins.size());
  ASSERT_TRUE(origins.find(url::Origin::Create(kProtectedManifestURL)) !=
              origins.end());
  ASSERT_TRUE(origins.find(url::Origin::Create(kNormalManifestURL)) !=
              origins.end());
  ASSERT_TRUE(origins.find(url::Origin::Create(kSessionOnlyManifestURL)) !=
              origins.end());
}

TEST_F(ChromeAppCacheServiceTest, KeepOnDestruction) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath appcache_path =
      temp_dir_.GetPath().Append(kTestingAppCacheDirname);

  // Create a ChromeAppCacheService and insert data into it
  scoped_refptr<ChromeAppCacheService> appcache_service =
      CreateAppCacheServiceImpl(appcache_path, true);
  ASSERT_TRUE(base::PathExists(appcache_path));
  ASSERT_TRUE(base::PathExists(appcache_path.AppendASCII("Index")));
  InsertDataIntoAppCache(appcache_service.get());

  // Test: delete the ChromeAppCacheService
  appcache_service = nullptr;
  scoped_task_environment_.RunUntilIdle();

  // Recreate the appcache (for reading the data back)
  appcache_service = CreateAppCacheServiceImpl(appcache_path, false);

  // The directory is still there
  ASSERT_TRUE(base::PathExists(appcache_path));

  // The appcache data is also there, except the session-only origin.
  AppCacheTestHelper appcache_helper;
  std::set<url::Origin> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service.get(), &origins);
  EXPECT_EQ(2UL, origins.size());
  EXPECT_TRUE(origins.find(url::Origin::Create(kProtectedManifestURL)) !=
              origins.end());
  EXPECT_TRUE(origins.find(url::Origin::Create(kNormalManifestURL)) !=
              origins.end());
  EXPECT_TRUE(origins.find(url::Origin::Create(kSessionOnlyManifestURL)) ==
              origins.end());

  // Delete and let cleanup tasks run prior to returning.
  appcache_service = nullptr;
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(ChromeAppCacheServiceTest, SaveSessionState) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath appcache_path =
      temp_dir_.GetPath().Append(kTestingAppCacheDirname);

  // Create a ChromeAppCacheService and insert data into it
  scoped_refptr<ChromeAppCacheService> appcache_service =
      CreateAppCacheServiceImpl(appcache_path, true);
  ASSERT_TRUE(base::PathExists(appcache_path));
  ASSERT_TRUE(base::PathExists(appcache_path.AppendASCII("Index")));
  InsertDataIntoAppCache(appcache_service.get());

  // Save session state. This should bypass the destruction-time deletion.
  appcache_service->set_force_keep_session_state();

  // Test: delete the ChromeAppCacheService
  appcache_service = nullptr;
  scoped_task_environment_.RunUntilIdle();

  // Recreate the appcache (for reading the data back)
  appcache_service = CreateAppCacheServiceImpl(appcache_path, false);

  // The directory is still there
  ASSERT_TRUE(base::PathExists(appcache_path));

  // No appcache data was deleted.
  AppCacheTestHelper appcache_helper;
  std::set<url::Origin> origins;
  appcache_helper.GetOriginsWithCaches(appcache_service.get(), &origins);
  EXPECT_EQ(3UL, origins.size());
  EXPECT_TRUE(origins.find(url::Origin::Create(kProtectedManifestURL)) !=
              origins.end());
  EXPECT_TRUE(origins.find(url::Origin::Create(kNormalManifestURL)) !=
              origins.end());
  EXPECT_TRUE(origins.find(url::Origin::Create(kSessionOnlyManifestURL)) !=
              origins.end());

  // Delete and let cleanup tasks run prior to returning.
  appcache_service = nullptr;
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace content
