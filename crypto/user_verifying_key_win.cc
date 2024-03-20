// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <functional>
#include <utility>

#include <windows.foundation.h>
#include <windows.security.credentials.h>
#include <windows.security.cryptography.core.h>
#include <windows.storage.streams.h>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "base/win/winrt_storage_util.h"
#include "crypto/random.h"
#include "crypto/user_verifying_key.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Security::Credentials::IKeyCredential;
using ABI::Windows::Security::Credentials::IKeyCredentialManagerStatics;
using ABI::Windows::Security::Credentials::IKeyCredentialOperationResult;
using ABI::Windows::Security::Credentials::IKeyCredentialRetrievalResult;
using ABI::Windows::Security::Credentials::KeyCredentialCreationOption;
using ABI::Windows::Security::Credentials::KeyCredentialOperationResult;
using ABI::Windows::Security::Credentials::KeyCredentialRetrievalResult;
using ABI::Windows::Security::Credentials::KeyCredentialStatus;
using ABI::Windows::Security::Credentials::KeyCredentialStatus_Success;
using ABI::Windows::Security::Cryptography::Core::
    CryptographicPublicKeyBlobType_X509SubjectPublicKeyInfo;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;

namespace crypto {

namespace {

enum KeyCredentialManagerAvailability {
  kUnknown = 0,
  kAvailable = 1,
  kUnavailable = 2,
};

std::string FormatError(std::string message, HRESULT hr) {
  return base::StrCat(
      {message, " (hr = ", logging::SystemErrorCodeToString(hr), ")"});
}

// This helper splits OnceCallback three ways for use with `PostAsyncHandlers`,
// which has three separate paths to outcomes: Invoke a success callback, invoke
// an error callback, or return an error.
template <typename... Args>
std::tuple<base::OnceCallback<void(Args...)>,
           base::OnceCallback<void(Args...)>,
           base::OnceCallback<void(Args...)>>
SplitOnceCallbackIntoThree(base::OnceCallback<void(Args...)> callback) {
  auto first_split = base::SplitOnceCallback(std::move(callback));
  auto second_split = base::SplitOnceCallback(std::move(first_split.first));
  return {std::move(first_split.second), std::move(second_split.first),
          std::move(second_split.second)};
}

void OnSigningSuccess(
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback,
    ComPtr<IKeyCredentialOperationResult> sign_result) {
  KeyCredentialStatus status;
  HRESULT hr = sign_result->get_Status(&status);
  if (FAILED(hr) || status != KeyCredentialStatus_Success) {
    LOG(ERROR) << FormatError(
        "Failed to obtain Status from IKeyCredentialOperationResult", hr);
    std::move(callback).Run(std::nullopt);
    return;
  }

  ComPtr<IBuffer> signature_buffer;
  hr = sign_result->get_Result(&signature_buffer);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "Failed to obtain Result from IKeyCredentialOperationResult", hr);
    std::move(callback).Run(std::nullopt);
    return;
  }

  uint8_t* signature_data = nullptr;
  uint32_t signature_length = 0;
  hr = base::win::GetPointerToBufferData(signature_buffer.Get(),
                                         &signature_data, &signature_length);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("Failed to obtain data from signature buffer",
                              hr);
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(
      std::vector<uint8_t>(signature_data, signature_data + signature_length));
}

void OnSigningError(
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback,
    HRESULT hr) {
  LOG(ERROR) << FormatError("Failed to sign with user-verifying signature", hr);
  std::move(callback).Run(std::nullopt);
}

void SignInternal(
    std::vector<uint8_t> data,
    ComPtr<IKeyCredential> credential,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback) {
  Microsoft::WRL::ComPtr<IBuffer> signing_buf;
  HRESULT hr =
      base::win::CreateIBufferFromData(data.data(), data.size(), &signing_buf);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("SignInternal: IBuffer creation failed", hr);
    std::move(callback).Run(std::nullopt);
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialOperationResult*>> sign_result;
  hr = credential->RequestSignAsync(signing_buf.Get(), &sign_result);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("SignInternal: Call to RequestSignAsync failed",
                              hr);
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto callback_splits = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      sign_result.Get(),
      base::BindOnce(&OnSigningSuccess,
                     std::move(std::get<0>(callback_splits))),
      base::BindOnce(&OnSigningError, std::move(std::get<1>(callback_splits))));
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("SignInternal: Call to PostAsyncHandlers failed",
                              hr);
    std::move(std::get<2>(callback_splits)).Run(std::nullopt);
    return;
  }
}

class UserVerifyingSigningKeyWin : public UserVerifyingSigningKey {
 public:
  UserVerifyingSigningKeyWin(std::string key_name,
                             ComPtr<IKeyCredential> credential)
      : key_name_(std::move(key_name)), credential_(std::move(credential)) {}
  ~UserVerifyingSigningKeyWin() override = default;

  void Sign(base::span<const uint8_t> data,
            base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                callback) override {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    std::vector<uint8_t> vec_data(data.begin(), data.end());
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SignInternal, std::move(vec_data), credential_,
            base::BindPostTaskToCurrentDefault(std::move(callback))));
  }

  std::vector<uint8_t> GetPublicKey() const override {
    ComPtr<IBuffer> key_buf;
    HRESULT hr = credential_->RetrievePublicKeyWithBlobType(
        CryptographicPublicKeyBlobType_X509SubjectPublicKeyInfo, &key_buf);
    CHECK(SUCCEEDED(hr)) << FormatError(
        "Failed to obtain public key from KeyCredential", hr);

    uint8_t* pub_key_data = nullptr;
    uint32_t pub_key_length = 0;
    hr = base::win::GetPointerToBufferData(key_buf.Get(), &pub_key_data,
                                           &pub_key_length);
    CHECK(SUCCEEDED(hr)) << FormatError(
        "Failed to access public key buffer data", hr);
    return std::vector<uint8_t>(pub_key_data, pub_key_data + pub_key_length);
  }

  const UserVerifyingKeyLabel& GetKeyLabel() const override {
    return key_name_;
  }

 private:
  std::string key_name_;
  ComPtr<IKeyCredential> credential_;
};

void OnKeyCreationCompletionSuccess(
    base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)> callback,
    std::string key_name,
    ComPtr<IKeyCredentialRetrievalResult> key_result) {
  KeyCredentialStatus status;
  HRESULT hr = key_result->get_Status(&status);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "Failed to obtain Status from IKeyCredentialRetrievalResult", hr);
    std::move(callback).Run(nullptr);
    return;
  } else if (status != KeyCredentialStatus_Success) {
    LOG(ERROR) << "IKeyCredentialRetrievalResult failed with status "
               << static_cast<uint32_t>(status);
    std::move(callback).Run(nullptr);
    return;
  }

  ComPtr<IKeyCredential> credential;
  hr = key_result->get_Credential(&credential);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "Failed to obtain KeyCredential from KeyCredentialRetrievalResult", hr);
    std::move(callback).Run(nullptr);
    return;
  }
  auto key = std::make_unique<UserVerifyingSigningKeyWin>(
      std::move(key_name), std::move(credential));
  std::move(callback).Run(std::move(key));
}

void OnKeyCreationCompletionError(
    base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)> callback,
    HRESULT hr) {
  LOG(ERROR) << FormatError("Failed to obtain user-verifying key from system",
                            hr);
  std::move(callback).Run(nullptr);
}

void GenerateUserVerifyingSigningKeyInternal(
    std::string key_label,
    base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
        callback) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  auto key_name = base::win::ScopedHString::Create(key_label);

  ComPtr<IKeyCredentialManagerStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IKeyCredentialManagerStatics,
      RuntimeClass_Windows_Security_Credentials_KeyCredentialManager>(&factory);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GenerateUserVerifyingSigningKeyInternal: Failed to obtain activation "
        "factory for KeyCredentialManager",
        hr);
    std::move(callback).Run(nullptr);
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>> create_result;
  hr = factory->RequestCreateAsync(
      key_name.get(),
      KeyCredentialCreationOption::KeyCredentialCreationOption_ReplaceExisting,
      &create_result);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GenerateUserVerifyingSigningKeyInternal: Call to RequestCreateAsync "
        "failed",
        hr);
    std::move(callback).Run(nullptr);
    return;
  }

  auto callback_splits = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      create_result.Get(),
      base::BindOnce(&OnKeyCreationCompletionSuccess,
                     std::move(std::get<0>(callback_splits)),
                     std::move(key_label)),
      base::BindOnce(&OnKeyCreationCompletionError,
                     std::move(std::get<1>(callback_splits))));
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GenerateUserVerifyingSigningKeyInternal: Call to PostAsyncHandlers "
        "failed",
        hr);
    std::move(std::get<2>(callback_splits)).Run(nullptr);
    return;
  }
}

void GetUserVerifyingSigningKeyInternal(
    std::string key_label,
    base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
        callback) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  auto key_name = base::win::ScopedHString::Create(key_label);

  ComPtr<IKeyCredentialManagerStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IKeyCredentialManagerStatics,
      RuntimeClass_Windows_Security_Credentials_KeyCredentialManager>(&factory);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GetUserVerifyingSigningKeyInternal: Failed to obtain activation "
        "factory for KeyCredentialManager",
        hr);
    std::move(callback).Run(nullptr);
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>> open_result;
  hr = factory->OpenAsync(key_name.get(), &open_result);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GetUserVerifyingSigningKeyInternal: Call to OpenAsync failed", hr);
    std::move(callback).Run(nullptr);
    return;
  }

  auto callback_splits = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      open_result.Get(),
      base::BindOnce(&OnKeyCreationCompletionSuccess,
                     std::move(std::get<0>(callback_splits)),
                     std::move(key_label)),
      base::BindOnce(&OnKeyCreationCompletionError,
                     std::move(std::get<1>(callback_splits))));
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GetUserVerifyingSigningKeyInternal: Call to PostAsyncHandlers failed",
        hr);
    std::move(std::get<2>(callback_splits)).Run(nullptr);
    return;
  }
}

std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
    base::span<const SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  // Windows keys come in any algorithm you want, as long as it's RSA 2048.
  for (auto algorithm : acceptable_algorithms) {
    if (algorithm == SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256) {
      return algorithm;
    }
  }
  return std::nullopt;
}

class UserVerifyingKeyProviderWin : public UserVerifyingKeyProvider {
 public:
  UserVerifyingKeyProviderWin() = default;
  ~UserVerifyingKeyProviderWin() override = default;

  void GenerateUserVerifyingSigningKey(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    // Ignore the non-empty return value of `SelectAlgorithm` unless in the
    // future Windows supports more algorithms.
    if (!SelectAlgorithm(acceptable_algorithms)) {
      LOG(ERROR) << "Key generation does not include a supported algorithm.";
      std::move(callback).Run(nullptr);
      return;
    }

    std::vector<uint8_t> random(16);
    crypto::RandBytes(random);
    UserVerifyingKeyLabel key_label =
        base::StrCat({"uvkey-", base::Base64Encode(random)});

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GenerateUserVerifyingSigningKeyInternal, std::move(key_label),
            base::BindPostTaskToCurrentDefault(std::move(callback))));
  }

  void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GetUserVerifyingSigningKeyInternal, key_label,
            base::BindPostTaskToCurrentDefault(std::move(callback))));
  }

  void DeleteUserVerifyingKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(bool)> callback) override {
    // TODO(crbug.com/40274370): implement.
    std::move(callback).Run(false);
  }
};

void IsKeyCredentialManagerAvailableInternal(
    base::OnceCallback<void(bool)> callback) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  // Lookup requires an asynchronous system API call, so cache the value.
  static std::atomic<KeyCredentialManagerAvailability> availability =
      KeyCredentialManagerAvailability::kUnknown;

  // Read once to ensure consistency.
  const KeyCredentialManagerAvailability current_availability = availability;
  if (current_availability != KeyCredentialManagerAvailability::kUnknown) {
    std::move(callback).Run(current_availability ==
                            KeyCredentialManagerAvailability::kAvailable);
    return;
  }

  ComPtr<IKeyCredentialManagerStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IKeyCredentialManagerStatics,
      RuntimeClass_Windows_Security_Credentials_KeyCredentialManager>(&factory);
  if (FAILED(hr)) {
    // Don't cache API call failures, allowing the possibility of trying again
    // if this was a one-time failure.
    std::move(callback).Run(false);
    return;
  }

  ComPtr<IAsyncOperation<bool>> is_supported_operation;
  hr = factory->IsSupportedAsync(&is_supported_operation);
  if (FAILED(hr)) {
    std::move(callback).Run(false);
    return;
  }

  auto callback_splits = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      is_supported_operation.Get(),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             std::atomic<KeyCredentialManagerAvailability>& availability,
             boolean result) {
            availability = result
                               ? KeyCredentialManagerAvailability::kAvailable
                               : KeyCredentialManagerAvailability::kUnavailable;
            std::move(callback).Run(result);
          },
          std::move(std::get<0>(callback_splits)), std::ref(availability)),
      base::BindOnce([](base::OnceCallback<void(bool)> callback,
                        HRESULT) { std::move(callback).Run(false); },
                     std::move(std::get<1>(callback_splits))));
  if (FAILED(hr)) {
    std::move(std::get<2>(callback_splits)).Run(false);
    return;
  }
}

}  // namespace

std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProviderWin() {
  return std::make_unique<UserVerifyingKeyProviderWin>();
}

void IsKeyCredentialManagerAvailable(base::OnceCallback<void(bool)> callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&IsKeyCredentialManagerAvailableInternal,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

}  // namespace crypto
