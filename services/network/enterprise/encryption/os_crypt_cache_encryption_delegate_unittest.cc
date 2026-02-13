// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/os_crypt_cache_encryption_delegate.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/cache_encryption_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::enterprise_encryption {

namespace {

constexpr char kProviderName[] = "test_provider";

// Creates a simple test key.
os_crypt_async::Encryptor::Key MakeTestKey() {
  std::vector<uint8_t> key(32, 1);
  return os_crypt_async::Encryptor::Key(
      key, os_crypt_async::mojom::Algorithm::kAES256GCM);
}

// Helper for creating an Encryptor for testing, since the constructor is
// protected.
class TestEncryptor : public os_crypt_async::Encryptor {
 public:
  TestEncryptor(
      KeyRing keys,
      const std::string& provider_for_encryption,
      const std::string& provider_for_os_crypt_sync_compatible_encryption)
      : Encryptor(std::move(keys),
                  provider_for_encryption,
                  provider_for_os_crypt_sync_compatible_encryption) {}
};

class MockCacheEncryptionProvider
    : public network::mojom::CacheEncryptionProvider {
 public:
  MockCacheEncryptionProvider() = default;
  ~MockCacheEncryptionProvider() override = default;

  // network::mojom::CacheEncryptionProvider implementation.
  void GetEncryptor(
      base::OnceCallback<void(os_crypt_async::Encryptor)> callback) override {
    if (call_get_encryptor_immediately_) {
      DoGetEncryptor(std::move(callback));
    } else {
      get_encryptor_callback_ = std::move(callback);
    }
  }

  // Returns a dummy key, which is not encrypted.
  void GetEncryptedCacheEncryptionKey(
      GetEncryptedCacheEncryptionKeyCallback callback) override {
    if (return_empty_key_) {
      std::move(callback).Run({});
      return;
    }
    std::move(callback).Run(std::vector<uint8_t>(32, 2));
  }

  void DoGetEncryptor(
      base::OnceCallback<void(os_crypt_async::Encryptor)> callback) {
    if (return_invalid_encryptor_) {
      std::move(callback).Run(
          TestEncryptor(os_crypt_async::Encryptor::KeyRing(), "", ""));
      return;
    }

    os_crypt_async::Encryptor::KeyRing keys;
    keys.emplace(kProviderName, MakeTestKey());
    std::move(callback).Run(
        TestEncryptor(std::move(keys), kProviderName, kProviderName));
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

 private:
  bool call_get_encryptor_immediately_ = true;
  bool return_invalid_encryptor_ = false;
  bool return_empty_key_ = false;
  base::OnceCallback<void(os_crypt_async::Encryptor)> get_encryptor_callback_;
};

class TestCacheEncryptionProviderWithRealEncryption
    : public network::mojom::CacheEncryptionProvider {
 public:
  TestCacheEncryptionProviderWithRealEncryption() {
    os_crypt_async::Encryptor::KeyRing keys;
    keys.emplace(kProviderName, MakeTestKey());
    TestEncryptor encryptor(std::move(keys), kProviderName, kProviderName);

    const std::string primary_key_plaintext = "my primary key";
    std::string primary_key_ciphertext;
    CHECK(encryptor.EncryptString(primary_key_plaintext,
                                  &primary_key_ciphertext));
    encrypted_primary_key_ = std::vector<uint8_t>(
        primary_key_ciphertext.begin(), primary_key_ciphertext.end());
  }
  ~TestCacheEncryptionProviderWithRealEncryption() override = default;

  void GetEncryptor(
      base::OnceCallback<void(os_crypt_async::Encryptor)> callback) override {
    os_crypt_async::Encryptor::KeyRing keys;
    keys.emplace(kProviderName, MakeTestKey());
    std::move(callback).Run(
        TestEncryptor(std::move(keys), kProviderName, kProviderName));
  }

  void GetEncryptedCacheEncryptionKey(
      GetEncryptedCacheEncryptionKeyCallback callback) override {
    std::move(callback).Run(encrypted_primary_key_);
  }

 private:
  std::vector<uint8_t> encrypted_primary_key_;
};

}  // namespace

class OSCryptCacheEncryptionDelegateTest : public testing::Test {
 public:
  OSCryptCacheEncryptionDelegateTest()
      : delegate_(provider_.BindNewPipeAndPassRemote()) {
    OSCryptMocker::SetUp();
  }

  ~OSCryptCacheEncryptionDelegateTest() override { OSCryptMocker::TearDown(); }

 protected:
  void InitDelegate() {
    base::RunLoop run_loop;
    int result_out = net::ERR_FAILED;
    delegate_.Init(base::BindLambdaForTesting([&](net::Error result) {
      result_out = result;
      run_loop.Quit();
    }));
    run_loop.Run();
    ASSERT_EQ(net::OK, result_out);
  }

  base::test::TaskEnvironment task_environment_;
  MockCacheEncryptionProvider mock_provider_;
  mojo::Receiver<network::mojom::CacheEncryptionProvider> provider_{
      &mock_provider_};
  OSCryptCacheEncryptionDelegate delegate_;
};

using OSCryptCacheEncryptionDelegateDeathTest =
    OSCryptCacheEncryptionDelegateTest;

TEST_F(OSCryptCacheEncryptionDelegateDeathTest, NotInitialized) {
#if defined(GTEST_HAS_DEATH_TEST)
  EXPECT_CHECK_DEATH(delegate_.GetEncryptionFileOperationsFactory(nullptr));
#endif  // defined(GTEST_HAS_DEATH_TEST)
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitFailsWithEmptyKey) {
  mock_provider_.set_return_empty_key(true);

  base::RunLoop run_loop;
  net::Error result_out = net::OK;
  delegate_.Init(base::BindLambdaForTesting([&](net::Error result) {
    result_out = result;
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_EQ(net::ERR_FAILED, result_out);
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitAfterInit) {
  InitDelegate();
  // Should just run the callback immediately with net::OK.
  net::Error result = net::ERR_FAILED;
  delegate_.Init(base::BindLambdaForTesting(
      [&](net::Error result_in) { result = result_in; }));
  EXPECT_EQ(net::OK, result);
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitWhileInitializing) {
  int result1 = net::ERR_FAILED;
  delegate_.Init(base::BindLambdaForTesting(
      [&](net::Error result_in) { result1 = result_in; }));

  int result2 = net::ERR_FAILED;
  delegate_.Init(base::BindLambdaForTesting(
      [&](net::Error result_in) { result2 = result_in; }));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(net::OK, result1);
  EXPECT_EQ(net::OK, result2);
}

TEST_F(OSCryptCacheEncryptionDelegateDeathTest, Disconnect) {
  InitDelegate();
  provider_.reset();
  task_environment_.RunUntilIdle();

#if defined(GTEST_HAS_DEATH_TEST)
  EXPECT_CHECK_DEATH(delegate_.GetEncryptionFileOperationsFactory(nullptr));
#endif  // defined(GTEST_HAS_DEATH_TEST)
}

TEST_F(OSCryptCacheEncryptionDelegateDeathTest, DisconnectDuringInit) {
  mock_provider_.set_call_get_encryptor_immediately(false);

  int result = net::OK;
  delegate_.Init(base::BindLambdaForTesting(
      [&](net::Error result_in) { result = result_in; }));

  // Disconnect. This should trigger the callback with an error.
  provider_.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, result);

  // Now, have the provider call its callback. This simulates the race.
  // The delegate should ignore this.
  mock_provider_.CallGetEncryptor();
  task_environment_.RunUntilIdle();

  // The delegate should still be uninitialized.
#if defined(GTEST_HAS_DEATH_TEST)
  EXPECT_CHECK_DEATH(delegate_.GetCacheEntryHasher());
#endif  // defined(GTEST_HAS_DEATH_TEST)
}

TEST_F(OSCryptCacheEncryptionDelegateDeathTest, InitEncryptorNotAvailable) {
  mock_provider_.set_return_invalid_encryptor(true);
#if BUILDFLAG(IS_APPLE)
  OSCryptMocker::SetBackendLocked(true);
#endif
  base::RunLoop run_loop;
  int result_out = net::OK;
  delegate_.Init(base::BindLambdaForTesting([&](net::Error result) {
    result_out = result;
    run_loop.Quit();
  }));
  run_loop.Run();

  if (result_out == net::OK) {
    // On platforms that do not support mocking OSCrypt as unavailable (e.g.
    // Windows), Init() will succeed via the fallback.
    EXPECT_FALSE(delegate_.GetEncryptionFileOperationsFactory(nullptr));
  } else {
    EXPECT_EQ(net::ERR_FAILED, result_out);
#if defined(GTEST_HAS_DEATH_TEST)
    EXPECT_CHECK_DEATH(delegate_.GetEncryptionFileOperationsFactory(nullptr));
#endif  // defined(GTEST_HAS_DEATH_TEST)
  }
}

TEST_F(OSCryptCacheEncryptionDelegateTest,
       GetEncryptionFileOperationsFactory_Success) {
  // Use a provider that returns a valid encrypted primary key.
  TestCacheEncryptionProviderWithRealEncryption provider;
  mojo::Receiver<network::mojom::CacheEncryptionProvider> receiver{&provider};
  OSCryptCacheEncryptionDelegate delegate(receiver.BindNewPipeAndPassRemote());

  base::RunLoop run_loop;
  net::Error result_out = net::ERR_FAILED;
  delegate.Init(base::BindLambdaForTesting([&](net::Error result) {
    result_out = result;
    run_loop.Quit();
  }));
  run_loop.Run();
  ASSERT_EQ(net::OK, result_out);

  EXPECT_NE(nullptr, delegate.GetEncryptionFileOperationsFactory(nullptr));
}

TEST_F(OSCryptCacheEncryptionDelegateTest,
       GetEncryptionFileOperationsFactory_FailsWhenKeyIsInvalid) {
  InitDelegate();
  // The default mock provider returns a key that cannot be decrypted.
  EXPECT_EQ(nullptr, delegate_.GetEncryptionFileOperationsFactory(nullptr));
}

TEST_F(OSCryptCacheEncryptionDelegateTest,
       GetCacheEntryHasher_FailsWhenKeyIsInvalid) {
  InitDelegate();
  // The default mock provider returns a key that cannot be decrypted.
  EXPECT_EQ(nullptr, delegate_.GetCacheEntryHasher());
}

}  // namespace network::enterprise_encryption
