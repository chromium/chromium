// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/os_crypt_cache_encryption_delegate.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cache_encryption_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_encryption {

namespace {

constexpr char kProviderName[] = "test_provider";

// Creates a simple test key.
os_crypt_async::Encryptor::Key MakeTestKey() {
  std::vector<uint8_t> key(32, 1);
  return os_crypt_async::Encryptor::Key(
      key, os_crypt_async::mojom::Algorithm::kAES256GCM);
}

// Helper for creating an Encryptor for testing.
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
    os_crypt_async::Encryptor::KeyRing keys;
    keys.emplace(kProviderName, MakeTestKey());
    std::move(callback).Run(
        TestEncryptor(std::move(keys), kProviderName, kProviderName));
  }
};

}  // namespace

class OSCryptCacheEncryptionDelegateTest : public testing::Test {
 public:
  OSCryptCacheEncryptionDelegateTest()
      : delegate_(provider_.BindNewPipeAndPassRemote()) {}

 protected:
  void InitDelegate() {
    base::RunLoop run_loop;
    delegate_.Init(run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  MockCacheEncryptionProvider mock_provider_;
  mojo::Receiver<network::mojom::CacheEncryptionProvider> provider_{
      &mock_provider_};
  OSCryptCacheEncryptionDelegate delegate_;
};

TEST_F(OSCryptCacheEncryptionDelegateTest, EncryptDecrypt) {
  InitDelegate();
  const std::vector<uint8_t> kPlaintext = {1, 2, 3, 4, 5};
  std::vector<uint8_t> ciphertext;
  ASSERT_TRUE(delegate_.EncryptData(kPlaintext, &ciphertext));
  EXPECT_NE(kPlaintext, ciphertext);

  std::vector<uint8_t> decrypted;
  ASSERT_TRUE(delegate_.DecryptData(ciphertext, &decrypted));
  EXPECT_EQ(kPlaintext, decrypted);
}

TEST_F(OSCryptCacheEncryptionDelegateTest, NotInitialized) {
  const std::vector<uint8_t> kPlaintext = {1, 2, 3, 4, 5};
  std::vector<uint8_t> ciphertext;
  EXPECT_FALSE(delegate_.EncryptData(kPlaintext, &ciphertext));

  std::vector<uint8_t> plaintext2;
  const std::vector<uint8_t> kCiphertext = {1, 2, 3, 4, 5, 6, 7};
  EXPECT_FALSE(delegate_.DecryptData(kCiphertext, &plaintext2));
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitAfterInit) {
  InitDelegate();
  // Should just run the callback immediately.
  bool ran = false;
  delegate_.Init(base::BindOnce([](bool* ran) { *ran = true; }, &ran));
  EXPECT_TRUE(ran);
}

TEST_F(OSCryptCacheEncryptionDelegateTest, InitWhileInitializing) {
  bool ran1 = false;
  delegate_.Init(base::BindOnce([](bool* ran) { *ran = true; }, &ran1));

  bool ran2 = false;
  delegate_.Init(base::BindOnce([](bool* ran) { *ran = true; }, &ran2));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(ran1);
  EXPECT_TRUE(ran2);
}

TEST_F(OSCryptCacheEncryptionDelegateTest, DecryptFail) {
  InitDelegate();
  // Encrypt something to get a valid ciphertext structure.
  const std::vector<uint8_t> kPlaintext = {1, 2, 3, 4, 5};
  std::vector<uint8_t> ciphertext;
  ASSERT_TRUE(delegate_.EncryptData(kPlaintext, &ciphertext));
  ASSERT_FALSE(ciphertext.empty());

  // Corrupt the ciphertext. This should cause decryption to fail.
  ciphertext.back() ^= 1;

  std::vector<uint8_t> plaintext;
  EXPECT_FALSE(delegate_.DecryptData(ciphertext, &plaintext));
}

TEST_F(OSCryptCacheEncryptionDelegateTest, Disconnect) {
  InitDelegate();
  const std::vector<uint8_t> kPlaintext = {1, 2, 3, 4, 5};
  std::vector<uint8_t> ciphertext;
  ASSERT_TRUE(delegate_.EncryptData(kPlaintext, &ciphertext));

  provider_.reset();
  task_environment_.RunUntilIdle();

  std::vector<uint8_t> plaintext2;
  EXPECT_FALSE(delegate_.DecryptData(ciphertext, &plaintext2));
}

}  // namespace enterprise_encryption
