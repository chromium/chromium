// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/os_crypt_cache_encryption_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_encryption_delegate.h"
#include "services/network/public/mojom/cache_encryption_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::enterprise_encryption {

namespace {

class TestCacheEncryptionProvider
    : public network::mojom::CacheEncryptionProvider {
 public:
  TestCacheEncryptionProvider()
      : oscrypt_async_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {}
  ~TestCacheEncryptionProvider() override = default;

  void GetEncryptor(
      base::OnceCallback<void(os_crypt_async::Encryptor)> callback) override {
    if (return_invalid_encryptor_) {
      std::move(callback).Run(
          os_crypt_async::GetTestEncryptorWithoutKeysForTesting());
      return;
    }
    if (call_get_encryptor_immediately_) {
      DoGetEncryptor(std::move(callback));
    } else {
      get_encryptor_callback_ = std::move(callback);
    }
  }

  void GetEncryptedCacheEncryptionKey(
      GetEncryptedCacheEncryptionKeyCallback callback) override {
    if (return_empty_key_) {
      std::move(callback).Run({});
      return;
    }
    oscrypt_async_->GetInstance(base::BindOnce(
        [](GetEncryptedCacheEncryptionKeyCallback callback,
           os_crypt_async::Encryptor encryptor) {
          const std::string primary_key_plaintext = "my primary key";
          std::string primary_key_ciphertext;
          CHECK(encryptor.EncryptString(primary_key_plaintext,
                                        &primary_key_ciphertext));
          std::move(callback).Run(std::vector<uint8_t>(
              primary_key_ciphertext.begin(), primary_key_ciphertext.end()));
        },
        std::move(callback)));
  }

  void DoGetEncryptor(
      base::OnceCallback<void(os_crypt_async::Encryptor)> callback) {
    oscrypt_async_->GetInstance(std::move(callback));
  }

  void CallGetEncryptor() {
    if (get_encryptor_callback_) {
      DoGetEncryptor(std::move(get_encryptor_callback_));
    }
  }

  void set_call_get_encryptor_immediately(bool value) {
    call_get_encryptor_immediately_ = value;
  }

  void set_return_invalid_encryptor(bool value) {
    return_invalid_encryptor_ = value;
  }

  void set_return_empty_key(bool value) { return_empty_key_ = value; }

  mojo::PendingRemote<network::mojom::CacheEncryptionProvider>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> oscrypt_async_;
  bool call_get_encryptor_immediately_ = true;
  bool return_invalid_encryptor_ = false;
  bool return_empty_key_ = false;
  base::OnceCallback<void(os_crypt_async::Encryptor)> get_encryptor_callback_;
  mojo::Receiver<network::mojom::CacheEncryptionProvider> receiver_{this};
};

}  // namespace

class OSCryptCacheEncryptionDelegateTest : public testing::Test {
 public:
  OSCryptCacheEncryptionDelegateTest()
      : delegate_(provider_.BindNewPipeAndPassRemote()) {}

  ~OSCryptCacheEncryptionDelegateTest() override = default;

 protected:
  void InitDelegate() {
    base::test::TestFuture<net::Error> future;
    delegate_.Init(future.GetCallback());
    EXPECT_EQ(net::OK, future.Get());
  }

  base::test::TaskEnvironment task_environment_;
  TestCacheEncryptionProvider provider_;
  OSCryptCacheEncryptionDelegate delegate_;
};

TEST_F(OSCryptCacheEncryptionDelegateTest, Init) {
  InitDelegate();
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitFailsWithEmptyKey) {
  TestCacheEncryptionProvider provider;
  provider.set_return_empty_key(true);
  OSCryptCacheEncryptionDelegate delegate(provider.BindNewPipeAndPassRemote());
  base::test::TestFuture<net::Error> future;
  delegate.Init(future.GetCallback());
  EXPECT_EQ(net::ERR_FAILED, future.Get());
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitAfterInit) {
  InitDelegate();
  base::test::TestFuture<net::Error> future;
  delegate_.Init(future.GetCallback());
  EXPECT_EQ(net::OK, future.Get());
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitWhileInitializing) {
  provider_.set_call_get_encryptor_immediately(false);
  base::test::TestFuture<net::Error> future1;
  base::test::TestFuture<net::Error> future2;
  delegate_.Init(future1.GetCallback());
  delegate_.Init(future2.GetCallback());

  // Ensure Init() Mojo calls reach the provider.
  task_environment_.RunUntilIdle();

  provider_.CallGetEncryptor();
  EXPECT_EQ(net::OK, future1.Get());
  EXPECT_EQ(net::OK, future2.Get());
}

using OSCryptCacheEncryptionDelegateDeathTest =
    OSCryptCacheEncryptionDelegateTest;

TEST_F(OSCryptCacheEncryptionDelegateDeathTest, NotInitialized) {
  // The delegate should still be uninitialized.
#if defined(GTEST_HAS_DEATH_TEST)
  EXPECT_CHECK_DEATH(delegate_.GetCacheEntryHasher());
#endif  // defined(GTEST_HAS_DEATH_TEST)
}

TEST_F(OSCryptCacheEncryptionDelegateDeathTest, InitEncryptorNotAvailable) {
  TestCacheEncryptionProvider provider;
  provider.set_return_invalid_encryptor(true);
  OSCryptCacheEncryptionDelegate delegate(provider.BindNewPipeAndPassRemote());
  base::test::TestFuture<net::Error> future;
  delegate.Init(future.GetCallback());

  EXPECT_EQ(net::ERR_FAILED, future.Get());
}

TEST_F(OSCryptCacheEncryptionDelegateTest, GetEncryptionFileOperationsFactory) {
  InitDelegate();
  EXPECT_TRUE(delegate_.GetEncryptionFileOperationsFactory(nullptr));
}

TEST_F(OSCryptCacheEncryptionDelegateTest, GetCacheEntryHasher) {
  InitDelegate();
  EXPECT_TRUE(delegate_.GetCacheEntryHasher());
}

}  // namespace network::enterprise_encryption
