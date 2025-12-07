// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_store.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/unexportable_keys/fake_unexportable_key_service.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

unexportable_keys::UnexportableKeyService* GetUnexportableKeyFactoryNull() {
  return nullptr;
}

class ScopedNullUnexportableKeyFactory {
 public:
  ScopedNullUnexportableKeyFactory() {
    UnexportableKeyServiceFactory::GetInstance()
        ->SetUnexportableKeyFactoryForTesting(GetUnexportableKeyFactoryNull);
  }
  ScopedNullUnexportableKeyFactory(const ScopedNullUnexportableKeyFactory&) =
      delete;
  ScopedNullUnexportableKeyFactory(ScopedNullUnexportableKeyFactory&&) = delete;
  ~ScopedNullUnexportableKeyFactory() {
    UnexportableKeyServiceFactory::GetInstance()
        ->SetUnexportableKeyFactoryForTesting(nullptr);
  }
};

class SessionStoreTest : public TestWithTaskEnvironment {
 protected:
  SessionStoreTest()
      : store_file_path_(base::FilePath(FILE_PATH_LITERAL("dummy_db_path"))) {}

  base::FilePath store_file_path() { return store_file_path_; }

 private:
  base::FilePath store_file_path_;
};

TEST_F(SessionStoreTest, HasStore) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
  auto store = SessionStore::Create(store_file_path(), nullptr);
  EXPECT_TRUE(store);
}

TEST_F(SessionStoreTest, NoStore) {
  // Empty db path not allowed.
  {
    crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
    auto store = SessionStore::Create(base::FilePath(), nullptr);
    EXPECT_FALSE(store);
  }
  // Null key service not allowed.
  {
    ScopedNullUnexportableKeyFactory null_factory;
    auto store = SessionStore::Create(store_file_path(), nullptr);
    EXPECT_FALSE(store);
  }
}

TEST_F(SessionStoreTest, HasUnexportableKeyService) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
  auto uks = unexportable_keys::FakeUnexportableKeyService();
  auto store = SessionStore::Create(store_file_path(), &uks);
  EXPECT_TRUE(store);
}
}  // namespace

}  // namespace net::device_bound_sessions
