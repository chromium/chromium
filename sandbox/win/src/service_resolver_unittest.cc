// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for ServiceResolverThunk.

#include "sandbox/win/src/service_resolver.h"

#include <stddef.h>

#include <memory>

#include "base/bit_cast.h"
#include "base/macros.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/resolver.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ResolverThunkTest {
 public:
  virtual ~ResolverThunkTest() {}

  virtual sandbox::ServiceResolverThunk* resolver() = 0;

  // Sets the interception target to the desired address.
  void set_target(void* target) { fake_target_ = target; }

 protected:
  // Holds the address of the fake target.
  void* fake_target_;
};

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll.
template <typename T>
class ResolverThunkTestImpl : public T, public ResolverThunkTest {
 public:
  // The service resolver needs a child process to write to.
  explicit ResolverThunkTestImpl(bool relaxed)
      : T(::GetCurrentProcess(), relaxed) {}

  sandbox::ServiceResolverThunk* resolver() { return this; }

 protected:
  // Overrides Resolver::Init
  virtual NTSTATUS Init(const void* target_module,
                        const void* interceptor_module,
                        const char* target_name,
                        const char* interceptor_name,
                        const void* interceptor_entry_point,
                        void* thunk_storage,
                        size_t storage_bytes) {
    NTSTATUS ret = STATUS_SUCCESS;
    ret = T::Init(target_module, interceptor_module, target_name,
                  interceptor_name, interceptor_entry_point, thunk_storage,
                  storage_bytes);
    EXPECT_EQ(STATUS_SUCCESS, ret);

    this->target_ = fake_target_;

    return ret;
  }

  DISALLOW_COPY_AND_ASSIGN(ResolverThunkTestImpl);
};

typedef ResolverThunkTestImpl<sandbox::ServiceResolverThunk> WinXpResolverTest;

#if !defined(_WIN64)
typedef ResolverThunkTestImpl<sandbox::Win8ResolverThunk> Win8ResolverTest;
typedef ResolverThunkTestImpl<sandbox::Wow64ResolverThunk> Wow64ResolverTest;
typedef ResolverThunkTestImpl<sandbox::Wow64W8ResolverThunk>
    Wow64W8ResolverTest;
typedef ResolverThunkTestImpl<sandbox::Wow64W10ResolverThunk>
    Wow64W10ResolverTest;
#endif

const BYTE kJump32 = 0xE9;

void CheckJump(void* source, void* target) {
#pragma pack(push)
#pragma pack(1)
  struct Code {
    BYTE jump;
    ULONG delta;
  };
#pragma pack(pop)

#if defined(_WIN64)
  FAIL() << "Running 32-bit codepath";
#else
  Code* patched = reinterpret_cast<Code*>(source);
  EXPECT_EQ(kJump32, patched->jump);

  ULONG source_addr = bit_cast<ULONG>(source);
  ULONG target_addr = bit_cast<ULONG>(target);
  EXPECT_EQ(target_addr + 19 - source_addr, patched->delta);
#endif
}

NTSTATUS PatchNtdllWithResolver(const char* function,
                                bool relaxed,
                                ResolverThunkTest* thunk_test) {
  HMODULE ntdll_base = ::GetModuleHandle(L"ntdll.dll");
  EXPECT_TRUE(ntdll_base);

  void* target =
      reinterpret_cast<void*>(::GetProcAddress(ntdll_base, function));
  EXPECT_TRUE(target);
  if (!target)
    return STATUS_UNSUCCESSFUL;

  BYTE service[50];
  memcpy(service, target, sizeof(service));

  thunk_test->set_target(service);

  sandbox::ServiceResolverThunk* resolver = thunk_test->resolver();
  // Any pointer will do as an interception_entry_point
  void* function_entry = resolver;
  size_t thunk_size = resolver->GetThunkSize();
  std::unique_ptr<char[]> thunk(new char[thunk_size]);
  size_t used;

  resolver->AllowLocalPatches();

  NTSTATUS ret =
      resolver->Setup(ntdll_base, nullptr, function, nullptr, function_entry,
                      thunk.get(), thunk_size, &used);
  if (NT_SUCCESS(ret)) {
    EXPECT_EQ(thunk_size, used);
    EXPECT_NE(0, memcmp(service, target, sizeof(service)));
    EXPECT_NE(kJump32, service[0]);

    if (relaxed) {
      // It's already patched, let's patch again, and simulate a direct patch.
      service[0] = kJump32;
      ret = resolver->Setup(ntdll_base, nullptr, function, nullptr,
                            function_entry, thunk.get(), thunk_size, &used);
      CheckJump(service, thunk.get());
    }
  }

  return ret;
}

std::unique_ptr<ResolverThunkTest> GetTestResolver(bool relaxed) {
#if defined(_WIN64)
  return std::make_unique<WinXpResolverTest>(relaxed);
#else
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->wow64_status() == base::win::OSInfo::WOW64_ENABLED) {
    if (os_info->version() >= base::win::Version::WIN10)
      return std::make_unique<Wow64W10ResolverTest>(relaxed);
    if (os_info->version() >= base::win::Version::WIN8)
      return std::make_unique<Wow64W8ResolverTest>(relaxed);
    return std::make_unique<Wow64ResolverTest>(relaxed);
  }

  if (os_info->version() >= base::win::Version::WIN8)
    return std::make_unique<Win8ResolverTest>(relaxed);

  return std::make_unique<WinXpResolverTest>(relaxed);
#endif
}

NTSTATUS PatchNtdll(const char* function, bool relaxed) {
  std::unique_ptr<ResolverThunkTest> thunk_test = GetTestResolver(relaxed);
  return PatchNtdllWithResolver(function, relaxed, thunk_test.get());
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
  std::unique_ptr<ResolverThunkTest> thunk_test = GetTestResolver(true);
  NTSTATUS ret = PatchNtdllWithResolver("NtClose", true, thunk_test.get());
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdllWithResolver("NtCreateFile", true, thunk_test.get());
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateFile, last error: " << ::GetLastError();

  ret = PatchNtdllWithResolver("NtCreateMutant", true, thunk_test.get());
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtCreateMutant, last error: " << ::GetLastError();

  ret = PatchNtdllWithResolver("NtMapViewOfSection", true, thunk_test.get());
  EXPECT_EQ(STATUS_SUCCESS, ret)
      << "NtMapViewOfSection, last error: " << ::GetLastError();
#endif
}

TEST(ServiceResolverTest, LocalPatchesAllowed) {
  std::unique_ptr<ResolverThunkTest> thunk_test = GetTestResolver(true);

  HMODULE ntdll_base = ::GetModuleHandle(L"ntdll.dll");
  ASSERT_TRUE(ntdll_base);

  const char kFunctionName[] = "NtClose";

  void* target =
      reinterpret_cast<void*>(::GetProcAddress(ntdll_base, kFunctionName));
  ASSERT_TRUE(target);

  BYTE service[50];
  memcpy(service, target, sizeof(service));
  thunk_test->set_target(service);

  sandbox::ServiceResolverThunk* resolver = thunk_test->resolver();
  // Any pointer will do as an interception_entry_point
  void* function_entry = resolver;
  size_t thunk_size = resolver->GetThunkSize();
  std::unique_ptr<char[]> thunk(new char[thunk_size]);
  size_t used;

  NTSTATUS ret = STATUS_UNSUCCESSFUL;

  // First try patching without having allowed local patches.
  ret = resolver->Setup(ntdll_base, nullptr, kFunctionName, nullptr,
                        function_entry, thunk.get(), thunk_size, &used);
  EXPECT_FALSE(NT_SUCCESS(ret));

  // Now allow local patches and check that things work.
  resolver->AllowLocalPatches();
  ret = resolver->Setup(ntdll_base, nullptr, kFunctionName, nullptr,
                        function_entry, thunk.get(), thunk_size, &used);
  EXPECT_EQ(STATUS_SUCCESS, ret);
}

}  // namespace
