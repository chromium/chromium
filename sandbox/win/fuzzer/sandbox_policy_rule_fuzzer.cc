// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_types.h"

// We only use the first two params so don't need more.
constexpr size_t maxParams = 2;

// This fills policies with rules based on the current renderer sandbox in
// Chrome - the point isn't to test the /sandbox/ but to fuzz the rule matching.
std::unique_ptr<sandbox::PolicyBase> InitPolicy() {
  auto policy = std::make_unique<sandbox::PolicyBase>("");
  auto* config = policy->GetConfig();
  CHECK(config);

  auto result = config->SetFakeGdiInit();
  CHECK_EQ(result, sandbox::SBOX_ALL_OK);

  result = config->AllowFileAccess(sandbox::FileSemantics::kAllowAny,
                                   L"\\??\\pipe\\chrome.*");
  CHECK_EQ(result, sandbox::SBOX_ALL_OK);

  result = config->AllowFileAccess(sandbox::FileSemantics::kAllowReadonly,
                                   L"\\??\\pipe\\chrome.unused.*");
  CHECK_EQ(result, sandbox::SBOX_ALL_OK);

  result = config->AllowFileAccess(sandbox::FileSemantics::kAllowAny,
                                   L"\\??\\*.log");
  CHECK_EQ(result, sandbox::SBOX_ALL_OK);

  sandbox::BrokerServicesBase::FreezeTargetConfigForTesting(
      policy->GetConfig());
  return policy;
}

struct FakeParameterSet {
  sandbox::ArgType real_type_;
  raw_ptr<void> address_;
};

static_assert(sizeof(FakeParameterSet) == sizeof(sandbox::ParameterSet));

struct FakeCountedParameterSetBase {
  size_t count;
  struct FakeParameterSet params[maxParams];
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FakeCountedParameterSetBase params;

  static std::unique_ptr<sandbox::PolicyBase> policy = InitPolicy();

  if (size < 20) {
    return 0;
  }
  // We will take a sizeof(pointer) and a wchar_t string so need an even number.
  if (size % sizeof(wchar_t)) {
    return 0;
  }

  // As parameters are created by Chromium code the format of the variables must
  // be correct, and any wstrings will be validly null-terminated.

  FuzzedDataProvider data_provider(data, size);
  params.count = maxParams;
  // The rules expect a string in param[0] so we construct that last with the
  // remaining bytes.
  params.params[1].real_type_ = static_cast<sandbox::ArgType>(
      data_provider.ConsumeIntegralInRange<uint8_t>(
          sandbox::ArgType::WCHAR_TYPE, sandbox::ArgType::LAST_TYPE));
  auto pointed_at_bytes = data_provider.ConsumeBytes<uint8_t>(sizeof(void*));
  // These variables must  remain in scope past the EvalPolicy call later.
  std::wstring param_1_wstring;
  const unsigned char* param_data = nullptr;
  if (params.params[1].real_type_ == sandbox::ArgType::WCHAR_TYPE) {
    param_1_wstring =
        std::wstring(reinterpret_cast<wchar_t*>(pointed_at_bytes.data()),
                     pointed_at_bytes.size() / sizeof(wchar_t));

    param_data =
        reinterpret_cast<const unsigned char*>(param_1_wstring.c_str());
  } else {
    param_data = pointed_at_bytes.data();
  }
  params.params[1].address_ = static_cast<void*>(&param_data);

  // param[0] is usually the filename. It must be a valid terminated wstring.
  params.params[0].real_type_ = sandbox::ArgType::WCHAR_TYPE;
  auto string_bytes =
      data_provider.ConsumeBytes<uint8_t>(data_provider.remaining_bytes());
  std::wstring valid_wstr(reinterpret_cast<wchar_t*>(string_bytes.data()),
                          string_bytes.size() / sizeof(wchar_t));
  const wchar_t* wcharstar_variable = valid_wstr.c_str();
  params.params[0].address_ = static_cast<void*>(&wcharstar_variable);

  // Overlay the real type.
  sandbox::CountedParameterSetBase* real_params =
      (sandbox::CountedParameterSetBase*)&params;

  // We send the fuzzer generated data to every available policy rule.
  // Only some of the services will be registered, but it will
  // quickly skip those that have nothing registered.
  for (size_t i = 0; i < sandbox::kSandboxIpcCount; i++) {
    policy->EvalPolicy(static_cast<sandbox::IpcTag>(i), real_params);
  }

  return 0;
}
