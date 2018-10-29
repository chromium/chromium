// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::ASCIIToUTF16;

namespace content {

class DOMStorageContextImplTest : public testing::Test {
 public:
  DOMStorageContextImplTest()
      : kOrigin(url::Origin::Create(GURL("http://dom_storage/"))),
        kKey(ASCIIToUTF16("key")),
        kValue(ASCIIToUTF16("value")),
        kDontIncludeFileInfo(false),
        kDoIncludeFileInfo(true) {}

  const url::Origin kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const bool kDontIncludeFileInfo;
  const bool kDoIncludeFileInfo;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    storage_policy_ = new MockSpecialStoragePolicy;
    task_runner_ =
        new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get());
    context_ = new DOMStorageContextImpl(
        base::FilePath(), storage_policy_.get(), task_runner_.get());
  }

  void TearDown() override {
    if (context_)
      context_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<MockDOMStorageTaskRunner> task_runner_;
  scoped_refptr<DOMStorageContextImpl> context_;
  DISALLOW_COPY_AND_ASSIGN(DOMStorageContextImplTest);
};

TEST_F(DOMStorageContextImplTest, Basics) {
  // This test doesn't do much, checks that the constructor
  // initializes members properly and that invoking methods
  // on a newly created object w/o any data on disk do no harm.
  EXPECT_EQ(base::FilePath(), context_->sessionstorage_directory());
  EXPECT_EQ(storage_policy_.get(), context_->special_storage_policy_.get());
  context_->Shutdown();
}

TEST_F(DOMStorageContextImplTest, PersistentIds) {
  const std::string kFirstNamespaceId = "persistent";
  context_->CreateSessionNamespace(kFirstNamespaceId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kFirstNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kFirstNamespaceId, dom_namespace->namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kFirstNamespaceId, area->namespace_id_);

  // Verify that the persistent IDs are handled correctly when cloning.
  const std::string kSecondNamespaceId = "cloned";
  context_->CloneSessionNamespace(kFirstNamespaceId, kSecondNamespaceId);
  DOMStorageNamespace* cloned_dom_namespace =
      context_->GetStorageNamespace(kSecondNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kSecondNamespaceId, cloned_dom_namespace->namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* cloned_area = cloned_dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kSecondNamespaceId, cloned_area->namespace_id_);
}

// Disable this test on Android as on Android we always delete our old session
// storage on startup. This is because we don't do any session restoration for
// the android system. See crbug.com/770307.
#if defined(OS_ANDROID)
TEST_F(DOMStorageContextImplTest, DISABLED_DeleteSessionStorage) {
#else
TEST_F(DOMStorageContextImplTest, DeleteSessionStorage) {
#endif
  // Create a DOMStorageContextImpl which will save sessionStorage on disk.
  context_->Shutdown();
  context_ = new DOMStorageContextImpl(
      temp_dir_.GetPath(), storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();
  ASSERT_EQ(temp_dir_.GetPath(), context_->sessionstorage_directory());

  // Write data.
  const std::string kFirstNamespaceId = "persistent";
  context_->CreateSessionNamespace(kFirstNamespaceId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kFirstNamespaceId);
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  const base::string16 kKey(ASCIIToUTF16("foo"));
  const base::string16 kValue(ASCIIToUTF16("bar"));
  base::NullableString16 old_nullable_value;
  area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value);
  dom_namespace->CloseStorageArea(area);

  // Destroy and recreate the DOMStorageContextImpl.
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();
  context_ = new DOMStorageContextImpl(
      temp_dir_.GetPath(), storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Read the data back.
  context_->CreateSessionNamespace(kFirstNamespaceId);
  dom_namespace = context_->GetStorageNamespace(kFirstNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);
  base::NullableString16 read_value;
  EXPECT_EQ(kKey, area->Key(0).string());
  dom_namespace->CloseStorageArea(area);

  SessionStorageUsageInfo info;
  info.origin = kOrigin.GetURL();
  info.namespace_id = kFirstNamespaceId;
  context_->DeleteSessionStorage(info);

  // Destroy and recreate again.
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();
  context_ = new DOMStorageContextImpl(
      temp_dir_.GetPath(), storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Now there should be no data.
  context_->CreateSessionNamespace(kFirstNamespaceId);
  dom_namespace = context_->GetStorageNamespace(kFirstNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);

  EXPECT_EQ(0u, area->Length());
  dom_namespace->CloseStorageArea(area);
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
