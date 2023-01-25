// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for ServiceResolverThunk.

#include "sandbox/win/src/service_resolver.h"

#include <ntstatus.h>
#include <stddef.h>

#include <memory>

#include "base/bit_cast.h"
#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll.
class ServiceResolverTest : public sandbox::ServiceResolverThunk {
 public:
  // The service resolver needs a child process to write to.
  explicit ServiceResolverTest(bool relaxed)
      : sandbox::ServiceResolverThunk(::GetCurrentProcess(), relaxed) {}

  ServiceResolverTest(const ServiceResolverTest&) = delete;
  ServiceResolverTest& operator=(const ServiceResolverTest&) = delete;

  // Sets the interception target to the desired address.
  void set_target(void* target) { fake_target_ = target; }

 protected:
  // Overrides Resolver::Init
  NTSTATUS Init(const void* target_module,
                const void* interceptor_module,
                const char* target_name,
                const char* interceptor_name,
                const void* interceptor_entry_point,
                void* thunk_storage,
                size_t storage_bytes) final {
    NTSTATUS ret = STATUS_SUCCESS;
    ret = sandbox::ServiceResolverThunk::Init(
        target_module, interceptor_module, target_name, interceptor_name,
        interceptor_entry_point, thunk_storage, storage_bytes);
    EXPECT_EQ(STATUS_SUCCESS, ret);

    this->target_ = fake_target_;

    return ret;
  }

  // Holds the address of the fake target.
  raw_ptr<void> fake_target_;
};

NTSTATUS PatchNtdllWithResolver(const char* function,
                                bool relaxed,
                                ServiceResolverTest& resolver) {
  HMODULE ntdll_base = ::GetModuleHandle(L"ntdll.dll");
  EXPECT_TRUE(ntdll_base);

  void* target =
      reinterpret_cast<void*>(::GetProcAddress(ntdll_base, function));
  EXPECT_TRUE(target);
  if (!target)
    return STATUS_UNSUCCESSFUL;

  BYTE service[50];
  memcpy(service, target, sizeof(service));

  resolver.set_target(service);

  // Any pointer will do as an interception_entry_point
  void* function_entry = &resolver;
  size_t thunk_size = resolver.GetThunkSize();
  std::unique_ptr<char[]> thunk = std::make_unique<char[]>(thunk_size);
  size_t used;

  resolver.AllowLocalPatches();

  NTSTATUS ret = resolver.Setup(ntdll_base, nullptr, function, nullptr,
                                function_entry, thunk.get(), thunk_size, &used);
  if (NT_SUCCESS(ret)) {
    const BYTE kJump32 = 0xE9;
    EXPECT_EQ(thunk_size, used);
    EXPECT_NE(0, memcmp(service, target, sizeof(service)));
    EXPECT_NE(kJump32, service[0]);

    if (relaxed) {
      // It's already patched, let's patch again, and simulate a direct patch.
      service[0] = kJump32;
      ret = resolver.Setup(ntdll_base, nullptr, function, nullptr,
                           function_entry, thunk.get(), thunk_size, &used);
      EXPECT_TRUE(resolver.VerifyJumpTargetForTesting(thunk.get()));
    }
  }

  return ret;
}

NTSTATUS PatchNtdll(const char* function, bool relaxed) {
  ServiceResolverTest resolver(relaxed);
  return PatchNtdllWithResolver(function, relaxed, resolver);
}

TEST(ServiceResolverTest, PatchesServices) {
  NTSTATUS ret = PatchNtdll("NtClose", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateFile", false);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateFile, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateMutant", false);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateMutant, last error: " << ::GetLastError();

  ret = PatchNtdll("NtMapViewOfSection", false);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtMapViewOfSection, last error: " << ::GetLastError();
}

TEST(ServiceResolverTest, FailsIfNotService) {
#if !defined(_WIN64)
  EXPECT_NE(STATUS_SUCCESS, PatchNtdll("RtlUlongByteSwap", false));
#endif

  EXPECT_NE(STATUS_SUCCESS, PatchNtdll("LdrLoadDll", false));
}

TEST(ServiceResolverTest, PatchesPatchedServices) {
// We don't support "relaxed mode" for Win64 apps.
#if !defined(_WIN64)
  NTSTATUS ret = PatchNtdll("NtClose", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateFile", true);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateFile, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateMutant", true);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateMutant, last error: " << ::GetLastError();

  ret = PatchNtdll("NtMapViewOfSection", true);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtMapViewOfSection, last error: " << ::GetLastError();
#endif
}

TEST(ServiceResolverTest, MultiplePatchedServices) {
// We don't support "relaxed mode" for Win64 apps.
#if !defined(_WIN64)
  ServiceResolverTest thunk_test(true);
  NTSTATUS ret = PatchNtdllWithResolver("NtClose", true, thunk_test);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdllWithResolver("NtCreateFile", true, thunk_test);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateFile, last error: " << ::GetLastError();

  ret = PatchNtdllWithResolver("NtCreateMutant", true, thunk_test);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateMutant, last error: " << ::GetLastError();

  ret = PatchNtdllWithResolver("NtMapViewOfSection", true, thunk_test);
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtMapViewOfSection, last error: " << ::GetLastError();
#endif
}

TEST(ServiceResolverTest, LocalPatchesAllowed) {
  ServiceResolverTest resolver(true);

  HMODULE ntdll_base = ::GetModuleHandle(L"ntdll.dll");
  ASSERT_TRUE(ntdll_base);

  const char kFunctionName[] = "NtClose";

  void* target =
      reinterpret_cast<void*>(::GetProcAddress(ntdll_base, kFunctionName));
  ASSERT_TRUE(target);

  BYTE service[50];
  memcpy(service, target, sizeof(service));
  resolver.set_target(service);

  // Any pointer will do as an interception_entry_point
  void* function_entry = &resolver;
  size_t thunk_size = resolver.GetThunkSize();
  std::unique_ptr<char[]> thunk = std::make_unique<char[]>(thunk_size);
  size_t used;

  NTSTATUS ret = STATUS_UNSUCCESSFUL;

  // First try patching without having allowed local patches.
  ret = resolver.Setup(ntdll_base, nullptr, kFunctionName, nullptr,
                       function_entry, thunk.get(), thunk_size, &used);
  EXPECT_FALSE(NT_SUCCESS(ret));

  // Now allow local patches and check that things work.
  resolver.AllowLocalPatches();
  ret = resolver.Setup(ntdll_base, nullptr, kFunctionName, nullptr,
                       function_entry, thunk.get(), thunk_size, &used);
  EXPECT_EQ(STATUS_SUCCESS, ret);
}

}  // namespace
