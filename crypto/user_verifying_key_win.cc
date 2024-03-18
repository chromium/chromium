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

// These helpers wrap callbacks by posting them to the original calling thread.
// This enables the wrapped callbacks to bind weak pointers.
template <typename Arg>
base::OnceCallback<void(Arg)> WrapOnceCallbackForCallingThread(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::OnceCallback<void(Arg)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         base::OnceCallback<void(Arg)> callback, Arg result) {
        caller_task_runner->PostTask(
            FROM_HERE,
            base::BindOnce([](base::OnceCallback<void(Arg)> callback,
                              Arg result) { std::move(callback).Run(result); },
                           std::move(callback), result));
      },
      caller_task_runner, std::move(callback));
}

template <typename Arg>
base::RepeatingCallback<void(Arg)> WrapRepeatingCallbackForCallingThread(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::RepeatingCallback<void(Arg)> callback) {
  return base::BindRepeating(
      [](scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         base::RepeatingCallback<void(Arg)> callback, Arg result) {
        caller_task_runner->PostTask(
            FROM_HERE,
            base::BindRepeating([](base::RepeatingCallback<void(Arg)> callback,
                                   Arg result) { callback.Run(result); },
                                std::move(callback), result));
      },
      caller_task_runner, std::move(callback));
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

void SignInternal(
    std::vector<uint8_t> data,
    ComPtr<IKeyCredential> credential,
    base::OnceCallback<void(ComPtr<IKeyCredentialOperationResult>)>
        success_callback,
    base::RepeatingCallback<void(HRESULT)> error_callback) {
  Microsoft::WRL::ComPtr<IBuffer> signing_buf;
  HRESULT hr =
      base::win::CreateIBufferFromData(data.data(), data.size(), &signing_buf);
  if (FAILED(hr)) {
    LOG(ERROR) << "SignInternal: IBuffer creation failed.";
    error_callback.Run(hr);
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialOperationResult*>> sign_result;
  hr = credential->RequestSignAsync(signing_buf.Get(), &sign_result);
  if (FAILED(hr)) {
    LOG(ERROR) << "SignInternal: Call to RequestSignAsync failed.";
    error_callback.Run(hr);
    return;
  }

  // Binds the IAsyncOperation to the callback to ensure it remains alive until
  // the callback is invoked.
  auto wrapped_success_callback = base::BindOnce(
      [](ComPtr<IAsyncOperation<KeyCredentialOperationResult*>>,
         base::OnceCallback<void(ComPtr<IKeyCredentialOperationResult>)>
             success_callback,
         ComPtr<IKeyCredentialOperationResult> result) {
        std::move(success_callback).Run(result);
      },
      sign_result, std::move(success_callback));

  hr = base::win::PostAsyncHandlers(
      sign_result.Get(), std::move(wrapped_success_callback),
      base::BindOnce([](ComPtr<IAsyncOperation<KeyCredentialOperationResult*>>,
                        base::RepeatingCallback<void(HRESULT)> cb,
                        HRESULT hr) { cb.Run(hr); },
                     sign_result, error_callback));
  if (FAILED(hr)) {
    LOG(ERROR) << "SignInternal: Call to PostAsyncHandlers failed.";
    error_callback.Run(hr);
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
    CHECK(!signing_callback_);
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    signing_callback_ = std::move(callback);
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    auto success_callback =
        WrapOnceCallbackForCallingThread<ComPtr<IKeyCredentialOperationResult>>(
            caller_task_runner,
            base::BindOnce(&UserVerifyingSigningKeyWin::OnSigningSuccess,
                           weak_factory_.GetWeakPtr()));
    auto error_callback = WrapRepeatingCallbackForCallingThread<HRESULT>(
        caller_task_runner,
        base::BindRepeating(&UserVerifyingSigningKeyWin::OnSigningError,
                            weak_factory_.GetWeakPtr()));
    std::vector<uint8_t> vec_data(data.begin(), data.end());
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&SignInternal, std::move(vec_data), credential_,
                       std::move(success_callback), std::move(error_callback)));
  }

  std::vector<uint8_t> GetPublicKey() const override {
    ComPtr<IBuffer> key_buf;
    HRESULT hr = credential_->RetrievePublicKeyWithBlobType(
        CryptographicPublicKeyBlobType_X509SubjectPublicKeyInfo, &key_buf);
    CHECK(SUCCEEDED(hr))
        << "Failed to obtain public key from KeyCredential, hr = "
        << logging::SystemErrorCodeToString(hr);

    uint8_t* pub_key_data = nullptr;
    uint32_t pub_key_length = 0;
    hr = base::win::GetPointerToBufferData(key_buf.Get(), &pub_key_data,
                                           &pub_key_length);
    CHECK(SUCCEEDED(hr)) << "Failed to access public key buffer data, hr = "
                         << logging::SystemErrorCodeToString(hr);
    return std::vector<uint8_t>(pub_key_data, pub_key_data + pub_key_length);
  }

  const UserVerifyingKeyLabel& GetKeyLabel() const override {
    return key_name_;
  }

 private:
  void OnSigningSuccess(ComPtr<IKeyCredentialOperationResult> sign_result) {
    // This SHOULD only be called once but conservatively we ignore additional
    // calls to reduce assumptions of good behaviour by the platform APIs.
    if (!signing_callback_) {
      return;
    }

    KeyCredentialStatus status;
    HRESULT hr = sign_result->get_Status(&status);
    if (FAILED(hr) || status != KeyCredentialStatus_Success) {
      LOG(ERROR) << "Failed to obtain Status from "
                    "IKeyCredentialOperationResult, hr = "
                 << logging::SystemErrorCodeToString(hr);
      std::move(signing_callback_).Run(std::nullopt);
      return;
    }

    ComPtr<IBuffer> signature_buffer;
    hr = sign_result->get_Result(&signature_buffer);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to obtain Result from "
                    "IKeyCredentialOperationResult, hr = "
                 << logging::SystemErrorCodeToString(hr);
      std::move(signing_callback_).Run(std::nullopt);
      return;
    }

    uint8_t* signature_data = nullptr;
    uint32_t signature_length = 0;
    hr = base::win::GetPointerToBufferData(signature_buffer.Get(),
                                           &signature_data, &signature_length);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to obtain data from "
                    "signature buffer, hr = "
                 << logging::SystemErrorCodeToString(hr);
      std::move(signing_callback_).Run(std::nullopt);
      return;
    }
    std::move(signing_callback_)
        .Run(std::vector<uint8_t>(signature_data,
                                  signature_data + signature_length));
  }

  void OnSigningError(HRESULT hr) {
    // This SHOULD only be called once but conservatively we ignore additional
    // calls to reduce assumptions of good behaviour by the platform APIs.
    if (!signing_callback_) {
      return;
    }
    LOG(ERROR) << "Failed to sign with user-verifying signature, hr = "
               << logging::SystemErrorCodeToString(hr);
    std::move(signing_callback_).Run(std::nullopt);
  }

  std::string key_name_;
  ComPtr<IKeyCredential> credential_;

  base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
      signing_callback_;

  base::WeakPtrFactory<UserVerifyingSigningKeyWin> weak_factory_{this};
};

void GenerateUserVerifyingSigningKeyInternal(
    base::win::ScopedHString key_name,
    base::OnceCallback<void(ComPtr<IKeyCredentialRetrievalResult>)>
        success_callback,
    base::RepeatingCallback<void(HRESULT)> error_callback) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  ComPtr<IKeyCredentialManagerStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IKeyCredentialManagerStatics,
      RuntimeClass_Windows_Security_Credentials_KeyCredentialManager>(&factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "GenerateUserVerifyingSigningKeyInternal: Failed to obtain "
                  "activation factory for KeyCredentialManager.";
    error_callback.Run(hr);
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>> create_result;
  hr = factory->RequestCreateAsync(
      key_name.get(),
      KeyCredentialCreationOption::KeyCredentialCreationOption_ReplaceExisting,
      &create_result);
  if (FAILED(hr)) {
    LOG(ERROR) << "GenerateUserVerifyingSigningKeyInternal: Call to "
                  "RequestCreateAsync failed.";
    error_callback.Run(hr);
    return;
  }

  // Binds the IAsyncOperation to the callback to ensure it remains alive until
  // the callback is invoked.
  auto wrapped_success_callback = base::BindOnce(
      [](ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>>,
         base::OnceCallback<void(ComPtr<IKeyCredentialRetrievalResult>)>
             success_callback,
         ComPtr<IKeyCredentialRetrievalResult> result) {
        std::move(success_callback).Run(result);
      },
      create_result, std::move(success_callback));

  hr = base::win::PostAsyncHandlers(
      create_result.Get(), std::move(wrapped_success_callback),
      base::BindOnce([](ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>>,
                        base::RepeatingCallback<void(HRESULT)> cb,
                        HRESULT hr) { cb.Run(hr); },
                     create_result, error_callback));
  if (FAILED(hr)) {
    LOG(ERROR) << "GenerateUserVerifyingSigningKeyInternal: Call to "
                  "PostAsyncHandlers failed.";
    error_callback.Run(hr);
    return;
  }
}

void GetUserVerifyingSigningKeyInternal(
    base::win::ScopedHString key_name,
    base::OnceCallback<void(ComPtr<IKeyCredentialRetrievalResult>)>
        success_callback,
    base::RepeatingCallback<void(HRESULT)> error_callback) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  ComPtr<IKeyCredentialManagerStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IKeyCredentialManagerStatics,
      RuntimeClass_Windows_Security_Credentials_KeyCredentialManager>(&factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "GetUserVerifyingSigningKeyInternal: Failed to obtain "
                  "activation factory for KeyCredentialManager.";
    error_callback.Run(hr);
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>> open_result;
  hr = factory->OpenAsync(key_name.get(), &open_result);
  if (FAILED(hr)) {
    LOG(ERROR)
        << "GetUserVerifyingSigningKeyInternal: Call to OpenAsync failed.";
    error_callback.Run(hr);
    return;
  }

  // Binds the IAsyncOperation to the callback to ensure it remains alive until
  // the callback is invoked.
  auto wrapped_success_callback = base::BindOnce(
      [](ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>>,
         base::OnceCallback<void(ComPtr<IKeyCredentialRetrievalResult>)>
             success_callback,
         ComPtr<IKeyCredentialRetrievalResult> result) {
        std::move(success_callback).Run(result);
      },
      open_result, std::move(success_callback));

  hr = base::win::PostAsyncHandlers(
      open_result.Get(), std::move(wrapped_success_callback),
      base::BindOnce([](ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>>,
                        base::RepeatingCallback<void(HRESULT)> cb,
                        HRESULT hr) { cb.Run(hr); },
                     open_result, error_callback));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetUserVerifyingSigningKeyInternal: Call to "
                  "PostAsyncHandlers failed.";
    error_callback.Run(hr);
    return;
  }
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
    CHECK(!key_creation_callback_);

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
    auto key_name = base::win::ScopedHString::Create(key_label);

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    key_creation_callback_ = std::move(callback);
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    auto success_callback =
        WrapOnceCallbackForCallingThread<ComPtr<IKeyCredentialRetrievalResult>>(
            caller_task_runner,
            base::BindOnce(
                &UserVerifyingKeyProviderWin::OnKeyCreationCompletionSuccess,
                weak_factory_.GetWeakPtr(), std::move(key_label)));
    auto error_callback = WrapRepeatingCallbackForCallingThread<HRESULT>(
        caller_task_runner,
        base::BindRepeating(
            &UserVerifyingKeyProviderWin::OnKeyCreationCompletionError,
            weak_factory_.GetWeakPtr()));
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&GenerateUserVerifyingSigningKeyInternal,
                       std::move(key_name), std::move(success_callback),
                       std::move(error_callback)));
  }

  void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    CHECK(!key_creation_callback_);
    auto key_name = base::win::ScopedHString::Create(key_label);
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    key_creation_callback_ = std::move(callback);
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    auto success_callback =
        WrapOnceCallbackForCallingThread<ComPtr<IKeyCredentialRetrievalResult>>(
            caller_task_runner,
            base::BindOnce(
                &UserVerifyingKeyProviderWin::OnKeyCreationCompletionSuccess,
                weak_factory_.GetWeakPtr(), std::move(key_label)));
    auto error_callback = WrapRepeatingCallbackForCallingThread<HRESULT>(
        caller_task_runner,
        base::BindRepeating(
            &UserVerifyingKeyProviderWin::OnKeyCreationCompletionError,
            weak_factory_.GetWeakPtr()));
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&GetUserVerifyingSigningKeyInternal, std::move(key_name),
                       std::move(success_callback), std::move(error_callback)));
  }

  void DeleteUserVerifyingKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(bool)> callback) override {
    // TODO(crbug.com/40274370): implement.
    std::move(callback).Run(false);
  }

 private:
  void OnKeyCreationCompletionSuccess(
      std::string key_name,
      ComPtr<IKeyCredentialRetrievalResult> key_result) {
    // This SHOULD only be called once but conservatively we ignore additional
    // calls to reduce assumptions of good behaviour by the platform APIs.
    if (!key_creation_callback_) {
      return;
    }

    KeyCredentialStatus status;
    HRESULT hr = key_result->get_Status(&status);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to obtain Status from "
                    "IKeyCredentialRetrievalResult, hr = "
                 << logging::SystemErrorCodeToString(hr);
      std::move(key_creation_callback_).Run(nullptr);
      return;
    } else if (status != KeyCredentialStatus_Success) {
      LOG(ERROR) << "IKeyCredentialRetrievalResult status is "
                 << static_cast<uint32_t>(status);
      std::move(key_creation_callback_).Run(nullptr);
      return;
    }

    ComPtr<IKeyCredential> credential;
    hr = key_result->get_Credential(&credential);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to obtain KeyCredential from "
                    "KeyCredentialRetrievalResult, hr = "
                 << logging::SystemErrorCodeToString(hr);
      std::move(key_creation_callback_).Run(nullptr);
      return;
    }
    auto key = std::make_unique<UserVerifyingSigningKeyWin>(
        std::move(key_name), std::move(credential));
    std::move(key_creation_callback_).Run(std::move(key));
  }

  void OnKeyCreationCompletionError(HRESULT hr) {
    // This SHOULD only be called once but conservatively we ignore additional
    // calls to reduce assumptions of good behaviour by the platform APIs.
    if (!key_creation_callback_) {
      return;
    }
    LOG(ERROR) << "Failed to obtain user-verifying key from system, hr = "
               << logging::SystemErrorCodeToString(hr);
    std::move(key_creation_callback_).Run(nullptr);
  }

  // This has to be cached here rather than bound as an argument
  // `KeyCreationCallback` because we have to use multiple callbacks
  // internally. Windows async APIs take separate callbacks for success and
  // failure, and a `OnceCallback` can't be bound to both.
  base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
      key_creation_callback_;

  base::WeakPtrFactory<UserVerifyingKeyProviderWin> weak_factory_{this};
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

  // This splits the callback three ways because two need to be moved as
  // arguments to `PostAsyncHandlers`, and one can be invoked if that
  // function returns an error.
  auto callback_splits = base::SplitOnceCallback(std::move(callback));
  auto callback_error_splits =
      base::SplitOnceCallback(std::move(callback_splits.first));

  hr = base::win::PostAsyncHandlers(
      is_supported_operation.Get(),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             std::atomic<KeyCredentialManagerAvailability>& availability,
             ComPtr<IAsyncOperation<bool>>, boolean result) {
            availability = result
                               ? KeyCredentialManagerAvailability::kAvailable
                               : KeyCredentialManagerAvailability::kUnavailable;
            std::move(callback).Run(result);
          },
          std::move(callback_splits.second), std::ref(availability),
          is_supported_operation),
      base::BindOnce([](base::OnceCallback<void(bool)> callback,
                        ComPtr<IAsyncOperation<bool>>,
                        HRESULT) { std::move(callback).Run(false); },
                     std::move(callback_error_splits.first),
                     is_supported_operation));
  if (FAILED(hr)) {
    std::move(callback_error_splits.second).Run(false);
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
