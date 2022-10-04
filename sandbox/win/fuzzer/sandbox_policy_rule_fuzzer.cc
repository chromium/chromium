// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_types.h"

// We only use the first two params so don't need more.
constexpr size_t maxParams = 2;

// This fills policies with rules based on the current
// renderer sandbox in Chrome.
std::unique_ptr<sandbox::PolicyBase> InitPolicy() {
  auto policy = std::make_unique<sandbox::PolicyBase>("");
  auto* config = policy->GetConfig();

  auto result = config->AddRule(sandbox::SubSystem::kWin32kLockdown,
                                sandbox::Semantics::kFakeGdiInit, nullptr);
  if (result != sandbox::SBOX_ALL_OK)
    return nullptr;

  result = config->AddRule(sandbox::SubSystem::kFiles,
                           sandbox::Semantics::kFilesAllowAny,
                           L"\\??\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK)
    return nullptr;

  result = config->AddRule(sandbox::SubSystem::kNamedPipes,
                           sandbox::Semantics::kNamedPipesAllowAny,
                           L"\\\\.\\pipe\\chrome.nacl.*");
  if (result != sandbox::SBOX_ALL_OK)
    return nullptr;

  result = config->AddRule(sandbox::SubSystem::kNamedPipes,
                           sandbox::Semantics::kNamedPipesAllowAny,
                           L"\\\\.\\pipe\\chrome.sync.*");
  if (result != sandbox::SBOX_ALL_OK)
    return nullptr;

  sandbox::BrokerServicesBase::FreezeTargetConfigForTesting(
      policy->GetConfig());
  return policy;
}

struct FakeParameterSet {
  sandbox::ArgType real_type_;
  void* address_;
};

struct FakeCountedParameterSetBase {
  size_t count;
  struct FakeParameterSet params[maxParams];
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FakeCountedParameterSetBase params;

  static std::unique_ptr<sandbox::PolicyBase> policy = InitPolicy();

  if (size < 20)
    return 0;

  FuzzedDataProvider data_provider(data, size);
  params.count = maxParams;
  // The rules expect a string in param[0] so we construct that last with the
  // remaining bytes.
  params.params[1].real_type_ = static_cast<sandbox::ArgType>(
      data_provider.ConsumeIntegralInRange<uint8_t>(
          0, sandbox::ArgType::LAST_TYPE));
  auto pointed_at_value = data_provider.ConsumeBytes<uint8_t>(sizeof(void*));
  params.params[1].address_ = static_cast<void*>(pointed_at_value.data());

  // param[0] is usually the filename.
  params.params[0].real_type_ = sandbox::ArgType::WCHAR_TYPE;
  auto pointed_at_string =
      data_provider.ConsumeBytes<uint8_t>(data_provider.remaining_bytes());
  params.params[0].address_ = static_cast<void*>(pointed_at_string.data());

  // Overlay the real type.
  sandbox::CountedParameterSetBase* real_params =
      (sandbox::CountedParameterSetBase*)&params;

  // We send the fuzzer generated data to every available policy rule.
  // Only some of the services will be registered, but it will
  // quickly skip those that have nothing registered.
  for (size_t i = 0; i < sandbox::kMaxIpcTag; i++)
    policy->EvalPolicy(static_cast<sandbox::IpcTag>(i), real_params);

  return 0;
}
