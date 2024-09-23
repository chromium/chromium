// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "crypto/user_verifying_key.h"

#include <windows.h>

#include <windows.foundation.h>
#include <windows.security.credentials.h>
#include <windows.security.cryptography.core.h>
#include <windows.storage.streams.h>

#include <atomic>
#include <functional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "base/win/winrt_storage_util.h"
#include "crypto/random.h"

using ABI::Windows::Foundation::IAsyncAction;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Security::Credentials::IKeyCredential;
using ABI::Windows::Security::Credentials::IKeyCredentialManagerStatics;
using ABI::Windows::Security::Credentials::IKeyCredentialOperationResult;
using ABI::Windows::Security::Credentials::IKeyCredentialRetrievalResult;
using ABI::Windows::Security::Credentials::KeyCredentialCreationOption;
using ABI::Windows::Security::Credentials::KeyCredentialOperationResult;
using ABI::Windows::Security::Credentials::KeyCredentialRetrievalResult;
using ABI::Windows::Security::Credentials::KeyCredentialStatus;
using ABI::Windows::Security::Credentials::
    KeyCredentialStatus_CredentialAlreadyExists;
using ABI::Windows::Security::Credentials::KeyCredentialStatus_NotFound;
using ABI::Windows::Security::Credentials::KeyCredentialStatus_Success;
using ABI::Windows::Security::Credentials::KeyCredentialStatus_UserCanceled;
using ABI::Windows::Security::Credentials::
    KeyCredentialStatus_UserPrefersPassword;
using ABI::Windows::Security::Cryptography::Core::
    CryptographicPublicKeyBlobType_X509SubjectPublicKeyInfo;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;

namespace crypto {

namespace {

// Possible outcomes for WinRT API calls. These are recorded for signing
// and key creation.
// Do not delete or reorder entries, this must be kept in sync with the
// corresponding metrics enum.
enum class KeyCredentialCreateResult {
  kSucceeded = 0,
  kAPIReturnedError = 1,
  kNoActivationFactory = 2,
  kRequestCreateAsyncFailed = 3,
  kPostAsyncHandlersFailed = 4,
  kInvalidStatusReturned = 5,
  kInvalidResultReturned = 6,
  kInvalidCredentialReturned = 7,

  kMaxValue = 7,
};

enum class KeyCredentialSignResult {
  kSucceeded = 0,
  kAPIReturnedError = 1,
  kRequestSignAsyncFailed = 2,
  kPostAsyncHandlersFailed = 3,
  kIBufferCreationFailed = 4,
  kInvalidStatusReturned = 5,
  kInvalidResultReturned = 6,
  kInvalidSignatureBufferReturned = 7,

  kMaxValue = 7,
};

void RecordCreateAsyncResult(KeyCredentialCreateResult result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.Windows.KeyCredentialCreation", result);
}

void RecordSignAsyncResult(KeyCredentialSignResult result) {
  base::UmaHistogramEnumeration("WebAuthentication.Windows.KeyCredentialSign",
                                result);
}

// Due to a Windows bug (http://task.ms/49689617), the system UI for
// KeyCredentialManager appears under all other windows, at least when invoked
// from a Win32 app. Therefore this code polls the visible windows and
// foregrounds the correct window when it appears.
class HelloDialogForegrounder
    : public base::RefCountedThreadSafe<HelloDialogForegrounder> {
 public:
  HelloDialogForegrounder() = default;
  HelloDialogForegrounder(const HelloDialogForegrounder&) = delete;
  HelloDialogForegrounder& operator=(const HelloDialogForegrounder&) = delete;

  void Start() {
    CHECK_EQ(state_, State::kNotStarted);
    state_ = State::kPollingForFirstAppearance;
    BringHelloDialogToFront(/*iteration=*/0);
  }

  void Stop() { stopping_.Set(); }

 private:
  friend class base::RefCountedThreadSafe<HelloDialogForegrounder>;
  ~HelloDialogForegrounder() = default;

  // Values to report the results of attempts to bring the Windows Hello
  // user verification dialog to the foreground.
  // Do not delete or reorder entries, this must be kept in sync with the
  // corresponding metrics enum.
  enum class ForegroundHelloDialogResult {
    kSucceeded = 0,
    kForegroundingFailed = 1,
    kWindowNotFound = 2,
    kAbortedWithoutFindingWindow = 3,

    kMaxValue = 3,
  };

  enum class State {
    kNotStarted,
    kPollingForFirstAppearance,
    kPollingForAuthRetry,
  };

  void RecordForegroundingOutcome(ForegroundHelloDialogResult result) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.Windows.ForegroundedWindowsHelloDialog", result);
  }

  void BringHelloDialogToFront(int iteration) {
    int interval = 100;

    if (stopping_.IsSet()) {
      if (state_ == State::kPollingForFirstAppearance) {
        // In State::kPollingForAuthRetry, success has already been reported.
        RecordForegroundingOutcome(
            ForegroundHelloDialogResult::kAbortedWithoutFindingWindow);
      }
      return;
    }

    constexpr wchar_t kTargetWindowName[] = L"Windows Security";
    constexpr wchar_t kTargetClassName[] = L"Credential Dialog Xaml Host";
    if (state_ == State::kPollingForFirstAppearance) {
      constexpr int kMaxIterations = 40;
      if (iteration > kMaxIterations) {
        RecordForegroundingOutcome(
            ForegroundHelloDialogResult::kWindowNotFound);
        return;
      }

      if (HWND hwnd = FindWindowW(kTargetClassName, kTargetWindowName)) {
        base::UmaHistogramExactLinear(
            "WebAuthentication.Windows.FindHelloDialogIterationCount",
            iteration,
            /*exclusive_max=*/kMaxIterations + 1);
        if (SetForegroundWindow(hwnd)) {
          RecordForegroundingOutcome(ForegroundHelloDialogResult::kSucceeded);
        } else {
          RecordForegroundingOutcome(
              ForegroundHelloDialogResult::kForegroundingFailed);
        }
        state_ = State::kPollingForAuthRetry;
      }
    } else {
      CHECK_EQ(state_, State::kPollingForAuthRetry);
      if (HWND hwnd = FindWindowW(kTargetClassName, kTargetWindowName)) {
        SetForegroundWindow(hwnd);
      }
      interval = 500;
    }
    base::ThreadPool::PostDelayedTask(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        base::BindOnce(&HelloDialogForegrounder::BringHelloDialogToFront,
                       base::WrapRefCounted<HelloDialogForegrounder>(this),
                       iteration + 1),
        base::Milliseconds(interval));
  }

  State state_ = State::kNotStarted;
  base::AtomicFlag stopping_;
};

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
    UserVerifyingSigningKey::UserVerifyingKeySignatureCallback callback,
    scoped_refptr<HelloDialogForegrounder> foregrounder,
    ComPtr<IKeyCredentialOperationResult> sign_result) {
  foregrounder->Stop();

  KeyCredentialStatus status;
  HRESULT hr = sign_result->get_Status(&status);
  if (FAILED(hr) || status != KeyCredentialStatus_Success) {
    LOG(ERROR) << FormatError(
        "Failed to obtain Status from IKeyCredentialOperationResult", hr);
    RecordSignAsyncResult(KeyCredentialSignResult::kInvalidStatusReturned);
    UserVerifyingKeySigningError sign_error;
    switch (status) {
      case KeyCredentialStatus_UserCanceled:
      case KeyCredentialStatus_UserPrefersPassword:
        sign_error = UserVerifyingKeySigningError::kUserCancellation;
        break;
      default:
        sign_error = UserVerifyingKeySigningError::kUnknownError;
    }
    std::move(callback).Run(base::unexpected(sign_error));
    return;
  }

  ComPtr<IBuffer> signature_buffer;
  hr = sign_result->get_Result(&signature_buffer);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "Failed to obtain Result from IKeyCredentialOperationResult", hr);
    RecordSignAsyncResult(KeyCredentialSignResult::kInvalidResultReturned);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeySigningError::kPlatformApiError));
    return;
  }

  uint8_t* signature_data = nullptr;
  uint32_t signature_length = 0;
  hr = base::win::GetPointerToBufferData(signature_buffer.Get(),
                                         &signature_data, &signature_length);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("Failed to obtain data from signature buffer",
                              hr);
    RecordSignAsyncResult(
        KeyCredentialSignResult::kInvalidSignatureBufferReturned);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeySigningError::kPlatformApiError));
    return;
  }

  RecordSignAsyncResult(KeyCredentialSignResult::kSucceeded);
  std::move(callback).Run(base::ok(
      std::vector<uint8_t>(signature_data, signature_data + signature_length)));
}

void OnSigningError(
    UserVerifyingSigningKey::UserVerifyingKeySignatureCallback callback,
    scoped_refptr<HelloDialogForegrounder> foregrounder,
    HRESULT hr) {
  foregrounder->Stop();
  LOG(ERROR) << FormatError("Failed to sign with user-verifying signature", hr);
  RecordSignAsyncResult(KeyCredentialSignResult::kAPIReturnedError);
  std::move(callback).Run(
      base::unexpected(UserVerifyingKeySigningError::kPlatformApiError));
}

void SignInternal(
    std::vector<uint8_t> data,
    ComPtr<IKeyCredential> credential,
    UserVerifyingSigningKey::UserVerifyingKeySignatureCallback callback) {
  Microsoft::WRL::ComPtr<IBuffer> signing_buf;
  HRESULT hr =
      base::win::CreateIBufferFromData(data.data(), data.size(), &signing_buf);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("SignInternal: IBuffer creation failed", hr);
    RecordSignAsyncResult(KeyCredentialSignResult::kIBufferCreationFailed);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeySigningError::kPlatformApiError));
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialOperationResult*>> sign_result;
  hr = credential->RequestSignAsync(signing_buf.Get(), &sign_result);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("SignInternal: Call to RequestSignAsync failed",
                              hr);
    RecordSignAsyncResult(KeyCredentialSignResult::kRequestSignAsyncFailed);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeySigningError::kPlatformApiError));
    return;
  }

  auto foregrounder = base::MakeRefCounted<HelloDialogForegrounder>();
  auto callback_splits = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      sign_result.Get(),
      base::BindOnce(&OnSigningSuccess, std::move(std::get<0>(callback_splits)),
                     foregrounder),
      base::BindOnce(&OnSigningError, std::move(std::get<1>(callback_splits)),
                     foregrounder));
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError("SignInternal: Call to PostAsyncHandlers failed",
                              hr);
    RecordSignAsyncResult(KeyCredentialSignResult::kPostAsyncHandlersFailed);
    std::move(std::get<2>(callback_splits))
        .Run(base::unexpected(UserVerifyingKeySigningError::kPlatformApiError));
    return;
  }

  foregrounder->Start();
}

class UserVerifyingSigningKeyWin : public UserVerifyingSigningKey {
 public:
  UserVerifyingSigningKeyWin(std::string key_name,
                             ComPtr<IKeyCredential> credential)
      : key_name_(std::move(key_name)), credential_(std::move(credential)) {}
  ~UserVerifyingSigningKeyWin() override = default;

  void Sign(base::span<const uint8_t> data,
            UserVerifyingKeySignatureCallback callback) override {
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

  bool IsHardwareBacked() const override { return true; }

 private:
  std::string key_name_;
  ComPtr<IKeyCredential> credential_;
};

void OnKeyCreationCompletionSuccess(
    UserVerifyingKeyProvider::UserVerifyingKeyCreationCallback callback,
    std::string key_name,
    scoped_refptr<HelloDialogForegrounder> foregrounder,
    ComPtr<IKeyCredentialRetrievalResult> key_result) {
  if (foregrounder) {
    foregrounder->Stop();
  }

  KeyCredentialStatus status;
  HRESULT hr = key_result->get_Status(&status);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "Failed to obtain Status from IKeyCredentialRetrievalResult", hr);
    RecordCreateAsyncResult(KeyCredentialCreateResult::kInvalidStatusReturned);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
    return;
  } else if (status != KeyCredentialStatus_Success) {
    LOG(ERROR) << "IKeyCredentialRetrievalResult failed with status "
               << static_cast<uint32_t>(status);
    RecordCreateAsyncResult(KeyCredentialCreateResult::kInvalidResultReturned);
    UserVerifyingKeyCreationError uv_key_error;
    switch (status) {
      case KeyCredentialStatus_CredentialAlreadyExists:
        uv_key_error = UserVerifyingKeyCreationError::kDuplicateCredential;
        break;
      case KeyCredentialStatus_NotFound:
        uv_key_error = UserVerifyingKeyCreationError::kNotFound;
        break;
      case KeyCredentialStatus_UserCanceled:
      case KeyCredentialStatus_UserPrefersPassword:
        uv_key_error = UserVerifyingKeyCreationError::kUserCancellation;
        break;
      default:
        uv_key_error = UserVerifyingKeyCreationError::kUnknownError;
    }
    std::move(callback).Run(base::unexpected(uv_key_error));
    return;
  }

  ComPtr<IKeyCredential> credential;
  hr = key_result->get_Credential(&credential);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "Failed to obtain KeyCredential from KeyCredentialRetrievalResult", hr);
    RecordCreateAsyncResult(
        KeyCredentialCreateResult::kInvalidCredentialReturned);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
    return;
  }

  RecordCreateAsyncResult(KeyCredentialCreateResult::kSucceeded);
  auto key = std::make_unique<UserVerifyingSigningKeyWin>(
      std::move(key_name), std::move(credential));
  std::move(callback).Run(base::ok(std::move(key)));
}

void OnKeyCreationCompletionError(
    UserVerifyingKeyProvider::UserVerifyingKeyCreationCallback callback,
    scoped_refptr<HelloDialogForegrounder> foregrounder,
    HRESULT hr) {
  if (foregrounder) {
    foregrounder->Stop();
  }
  LOG(ERROR) << FormatError("Failed to obtain user-verifying key from system",
                            hr);
  RecordCreateAsyncResult(KeyCredentialCreateResult::kAPIReturnedError);
  std::move(callback).Run(
      base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
}

void GenerateUserVerifyingSigningKeyInternal(
    std::string key_label,
    UserVerifyingKeyProvider::UserVerifyingKeyCreationCallback callback) {
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
    RecordCreateAsyncResult(KeyCredentialCreateResult::kNoActivationFactory);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
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
    RecordCreateAsyncResult(
        KeyCredentialCreateResult::kRequestCreateAsyncFailed);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
    return;
  }

  auto foregrounder = base::MakeRefCounted<HelloDialogForegrounder>();
  auto callback_splits = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      create_result.Get(),
      base::BindOnce(&OnKeyCreationCompletionSuccess,
                     std::move(std::get<0>(callback_splits)),
                     std::move(key_label), foregrounder),
      base::BindOnce(&OnKeyCreationCompletionError,
                     std::move(std::get<1>(callback_splits)), foregrounder));
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GenerateUserVerifyingSigningKeyInternal: Call to PostAsyncHandlers "
        "failed",
        hr);
    RecordCreateAsyncResult(
        KeyCredentialCreateResult::kPostAsyncHandlersFailed);
    std::move(std::get<2>(callback_splits))
        .Run(
            base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
    return;
  }

  foregrounder->Start();
}

void GetUserVerifyingSigningKeyInternal(
    std::string key_label,
    UserVerifyingKeyProvider::UserVerifyingKeyCreationCallback callback) {
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
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
    return;
  }

  ComPtr<IAsyncOperation<KeyCredentialRetrievalResult*>> open_result;
  hr = factory->OpenAsync(key_name.get(), &open_result);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GetUserVerifyingSigningKeyInternal: Call to OpenAsync failed", hr);
    std::move(callback).Run(
        base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
    return;
  }

  auto callback_splits = SplitOnceCallbackIntoThree(std::move(callback));
  hr = base::win::PostAsyncHandlers(
      open_result.Get(),
      base::BindOnce(&OnKeyCreationCompletionSuccess,
                     std::move(std::get<0>(callback_splits)),
                     std::move(key_label),
                     /*HelloDialogForegrounder=*/nullptr),
      base::BindOnce(&OnKeyCreationCompletionError,
                     std::move(std::get<1>(callback_splits)),
                     /*HelloDialogForegrounder=*/nullptr));
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "GetUserVerifyingSigningKeyInternal: Call to PostAsyncHandlers failed",
        hr);
    std::move(std::get<2>(callback_splits))
        .Run(
            base::unexpected(UserVerifyingKeyCreationError::kPlatformApiError));
    return;
  }
}

void DeleteUserVerifyingKeyInternal(UserVerifyingKeyLabel key_label,
                                    base::OnceCallback<void(bool)> callback) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  ComPtr<IKeyCredentialManagerStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IKeyCredentialManagerStatics,
      RuntimeClass_Windows_Security_Credentials_KeyCredentialManager>(&factory);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "DeleteUserVerifyingKeyInternal: Failed to obtain activation "
        "factory for KeyCredentialManager",
        hr);
    std::move(callback).Run(false);
    return;
  }

  ComPtr<IAsyncAction> delete_operation;
  auto key_name = base::win::ScopedHString::Create(key_label);
  hr = factory->DeleteAsync(key_name.get(), &delete_operation);
  if (FAILED(hr)) {
    LOG(ERROR) << FormatError(
        "DeleteUserVerifyingKeyInternal: Call to DeleteAsync failed", hr);
    std::move(callback).Run(false);
    return;
  }

  // DeleteAsync does not report a value, so we have to assume success.
  std::move(callback).Run(true);
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
      UserVerifyingKeyCreationCallback callback) override {
    // Ignore the non-empty return value of `SelectAlgorithm` unless in the
    // future Windows supports more algorithms.
    if (!SelectAlgorithm(acceptable_algorithms)) {
      LOG(ERROR) << "Key generation does not include a supported algorithm.";
      std::move(callback).Run(base::unexpected(
          UserVerifyingKeyCreationError::kNoMatchingAlgorithm));
      return;
    }

    std::vector<uint8_t> random(16);
    crypto::RandBytes(random);
    UserVerifyingKeyLabel key_label =
        base::StrCat({"uvkey-", base::HexEncode(random)});

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
      UserVerifyingKeyCreationCallback callback) override {
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
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(DeleteUserVerifyingKeyInternal, key_label,
                                  base::BindPostTaskToCurrentDefault(
                                      std::move(callback))));
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
