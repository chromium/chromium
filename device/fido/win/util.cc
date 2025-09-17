// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/util.h"

#include <windows.foundation.h>
#include <windows.security.credentials.ui.h>
#include <wtsapi32.h>

#include "absl/cleanup/cleanup.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"

namespace device::fido::win {

namespace {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Security::Credentials::UI::IUserConsentVerifierStatics;
using ABI::Windows::Security::Credentials::UI::UserConsentVerifierAvailability;
using enum ABI::Windows::Security::Credentials::UI::
    UserConsentVerifierAvailability;
using Microsoft::WRL::ComPtr;

enum BiometricAvailability {
  kUnknown = 0,
  kAvailable = 1,
  kUnavailable = 2,
};

void DeviceHasBiometricsAvailableInternal(
    base::OnceCallback<void(bool)> callback) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  // Lookup requires an asynchronous system API call, so cache the value.
  static std::atomic<BiometricAvailability> availability =
      BiometricAvailability::kUnknown;

  // Read once from the atomic to ensure consistency.
  const BiometricAvailability current_availability = availability;
  if (current_availability != BiometricAvailability::kUnknown) {
    std::move(callback).Run(current_availability ==
                            BiometricAvailability::kAvailable);
    return;
  }

  ComPtr<IUserConsentVerifierStatics> factory;
  HRESULT hr = base::win::GetActivationFactory<
      IUserConsentVerifierStatics,
      RuntimeClass_Windows_Security_Credentials_UI_UserConsentVerifier>(
      &factory);
  if (FAILED(hr)) {
    std::move(callback).Run(false);
    return;
  }
  ComPtr<IAsyncOperation<UserConsentVerifierAvailability>> async_op;
  hr = factory->CheckAvailabilityAsync(&async_op);
  if (FAILED(hr)) {
    std::move(callback).Run(false);
    return;
  }

  // The OnceCallback has to be split three ways here, because there are three
  // paths to resolution: success, asynchronous failure (the operation reports
  // an error), synchronous failure (PostAsyncHandlers returns an error).
  auto first_split = base::SplitOnceCallback(std::move(callback));
  auto second_split = base::SplitOnceCallback(std::move(first_split.first));
  hr = base::win::PostAsyncHandlers(
      async_op.Get(),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             std::atomic<BiometricAvailability>& availability,
             UserConsentVerifierAvailability result) {
            boolean available =
                result == UserConsentVerifierAvailability_Available;
            if (result != UserConsentVerifierAvailability_DeviceBusy) {
              availability = available ? BiometricAvailability::kAvailable
                                       : BiometricAvailability::kUnavailable;
            }
            std::move(callback).Run(available);
          },
          std::move(first_split.second), std::ref(availability)),
      base::BindOnce([](base::OnceCallback<void(bool)> callback,
                        HRESULT) { std::move(callback).Run(false); },
                     std::move(second_split.first)));
  if (FAILED(hr)) {
    std::move(second_split.second).Run(false);
    return;
  }
}

std::optional<bool>& GetBiometricOverride() {
  static std::optional<bool> flag;
  return flag;
}

std::optional<bool>& GetRemoteDesktopSessionOverride() {
  static std::optional<bool> flag;
  return flag;
}

}  // namespace

ScopedBiometricsOverride::ScopedBiometricsOverride(bool has_biometrics) {
  std::optional<bool>& flag = GetBiometricOverride();
  // Overrides don't nest.
  CHECK(!flag.has_value());
  flag = has_biometrics;
}

ScopedBiometricsOverride::~ScopedBiometricsOverride() {
  std::optional<bool>& flag = GetBiometricOverride();
  CHECK(flag.has_value());
  flag.reset();
}

void DeviceHasBiometricsAvailable(base::OnceCallback<void(bool)> callback) {
  std::optional<bool>& flag = GetBiometricOverride();
  if (flag.has_value()) {
    std::move(callback).Run(*flag);
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DeviceHasBiometricsAvailableInternal,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

bool IsRemoteDesktopSession() {
  std::optional<bool>& flag = GetRemoteDesktopSessionOverride();
  if (flag.has_value()) {
    return *flag;
  }
  USHORT* result = nullptr;
  DWORD result_size;
  if (!WTSQuerySessionInformation(
          WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSClientProtocolType,
          reinterpret_cast<LPWSTR*>(&result), &result_size)) {
    // An error occurred. Assume this is not a remote desktop session.
    return false;
  }
  absl::Cleanup result_deleter = [result] { WTSFreeMemory(result); };
  // `WTSClientProtocolType` should make the API return an USHORT value.
  // Anything else is an API contract break, assume we are not under remote
  // desktop.
  if (result_size != sizeof(USHORT)) {
    return false;
  }
  return *result != WTS_PROTOCOL_TYPE_CONSOLE;
}

ScopedIsRdpSessionOverride::ScopedIsRdpSessionOverride(bool is_rdp) {
  std::optional<bool>& flag = GetRemoteDesktopSessionOverride();
  // Overrides don't nest.
  CHECK(!flag.has_value());
  flag = is_rdp;
}

ScopedIsRdpSessionOverride::~ScopedIsRdpSessionOverride() {
  std::optional<bool>& flag = GetRemoteDesktopSessionOverride();
  CHECK(flag.has_value());
  flag.reset();
}

}  // namespace device::fido::win
