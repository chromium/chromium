// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>

#include <ntstatus.h>
#include <stdlib.h>
#include <winternl.h>

#include <memory>

#include "base/memory/page_size.h"
#include "base/win/win_util.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/filesystem_interception.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/process_thread_interception.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

enum TestId {
  TESTIPC_NTOPENFILE,
  TESTIPC_NTCREATEFILE,
  TESTIPC_CREATETHREAD,
  TESTIPC_LAST
};

// Helper function to allocate space (on the heap) for policy.
PolicyGlobal* MakePolicyMemory() {
  // Should not exceed kPolMemSize from |sandbox_policy_base.cc|.
  const size_t kTotalPolicySz = 4096 * 6;
  char* mem = new char[kTotalPolicySz];
  memset(mem, 0, kTotalPolicySz);
  PolicyGlobal* policy = reinterpret_cast<PolicyGlobal*>(mem);
  policy->data_size = kTotalPolicySz - sizeof(PolicyGlobal);
  return policy;
}

// NtCreateFile
NTSTATUS WINAPI DummyNtCreateFile(PHANDLE file,
                                  ACCESS_MASK desired_access,
                                  POBJECT_ATTRIBUTES object_attributes,
                                  PIO_STATUS_BLOCK io_status,
                                  PLARGE_INTEGER allocation_size,
                                  ULONG file_attributes,
                                  ULONG sharing,
                                  ULONG disposition,
                                  ULONG options,
                                  PVOID ea_buffer,
                                  ULONG ea_length) {
  return STATUS_ACCESS_DENIED;
}

void TestNtCreateFile() {
  UNICODE_STRING path_str;
  OBJECT_ATTRIBUTES attr;
  IO_STATUS_BLOCK iosb;
  HANDLE handle = INVALID_HANDLE_VALUE;
  RtlInitUnicodeString(&path_str, L"\\??\\leak");
  InitializeObjectAttributes(&attr, &path_str, OBJ_CASE_INSENSITIVE, nullptr,
                             nullptr);
  NTSTATUS ret = TargetNtCreateFile(
      reinterpret_cast<NtCreateFileFunction>(DummyNtCreateFile), &handle,
      FILE_READ_DATA, &attr, &iosb, 0, 0,
      FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, FILE_OPEN, 0,
      nullptr, 0);
  if (NT_SUCCESS(ret))
    CloseHandle(handle);
}

// NtOpenFile
NTSTATUS WINAPI DummyNtOpenFile(PHANDLE file,
                                ACCESS_MASK desired_access,
                                POBJECT_ATTRIBUTES object_attributes,
                                PIO_STATUS_BLOCK io_status,
                                ULONG sharing,
                                ULONG options) {
  return STATUS_ACCESS_DENIED;
}

void TestNtOpenFile() {
  UNICODE_STRING path_str;
  OBJECT_ATTRIBUTES attr;
  IO_STATUS_BLOCK iosb;
  HANDLE handle = INVALID_HANDLE_VALUE;
  RtlInitUnicodeString(&path_str, L"\\??\\leak");
  InitializeObjectAttributes(&attr, &path_str, OBJ_CASE_INSENSITIVE, nullptr,
                             nullptr);
  NTSTATUS ret = TargetNtOpenFile(
      reinterpret_cast<NtOpenFileFunction>(DummyNtOpenFile), &handle,
      FILE_READ_DATA | SYNCHRONIZE, &attr, &iosb, FILE_SHARE_READ, FILE_OPEN);
  if (NT_SUCCESS(ret))
    CloseHandle(handle);
}

// CreateThread
HANDLE WINAPI DummyCreateThread(LPSECURITY_ATTRIBUTES thread_attributes,
                                SIZE_T stack_size,
                                LPTHREAD_START_ROUTINE start_address,
                                LPVOID parameter,
                                DWORD creation_flags,
                                LPDWORD thread_id) {
  return nullptr;
}

DWORD WINAPI ThreadEntry(LPVOID) {
  return 0;
}

void TestCreateThread() {
  HANDLE handle = TargetCreateThread(
      reinterpret_cast<CreateThreadFunction>(DummyCreateThread), nullptr,
      SIZE_MAX, ThreadEntry, nullptr, 0, nullptr);
  if (handle) {
    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
  }
}

// Generates a blank policy where all the rules are ASK_BROKER.
PolicyGlobal* GenerateBlankPolicy() {
  PolicyGlobal* policy = MakePolicyMemory();

  LowLevelPolicy policy_maker(policy);

  for (size_t i = 0; i < kSandboxIpcCount; i++) {
    IpcTag service = static_cast<IpcTag>(i);
    PolicyRule ask_broker(ASK_BROKER);
    ask_broker.Done();
    policy_maker.AddRule(service, &ask_broker);
  }

  policy_maker.Done();
  return policy;
}

// The Policy structure must be flattened before placing into the policy buffer.
// This code is taken from target_process.cc
void CopyPolicyToTarget(const void* source, size_t size, void* dest) {
  if (!source || !size)
    return;
  memcpy(dest, source, size);
  sandbox::PolicyGlobal* policy =
      reinterpret_cast<sandbox::PolicyGlobal*>(dest);

  size_t offset = reinterpret_cast<size_t>(source);

  for (size_t i = 0; i < kSandboxIpcCount; i++) {
    size_t buffer = reinterpret_cast<size_t>(policy->entry[i]);
    if (buffer) {
      buffer -= offset;
      policy->entry[i] = reinterpret_cast<sandbox::PolicyBuffer*>(buffer);
    }
  }
}

}  // namespace

SBOX_TESTS_COMMAND int IPC_Leak(int argc, wchar_t** argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED;

  // Replace current target policy with one that forwards all interceptions to
  // broker.
  PolicyGlobal* policy = GenerateBlankPolicy();
  PolicyGlobal* current_policy =
      (PolicyGlobal*)sandbox::GetGlobalPolicyMemoryForTesting();
  CopyPolicyToTarget(policy, policy->data_size + sizeof(PolicyGlobal),
                     current_policy);

  int test = wcstol(argv[0], nullptr, 10);

  static_assert(TESTIPC_NTOPENFILE == 0,
                "TESTIPC_NTOPENFILE must be first in enum.");
  if (test < TESTIPC_NTOPENFILE || test >= TESTIPC_LAST)
    return SBOX_TEST_INVALID_PARAMETER;

  auto test_id = TestId(test);

  switch (test_id) {
    case TESTIPC_NTOPENFILE:
      TestNtOpenFile();
      break;
    case TESTIPC_NTCREATEFILE:
      TestNtCreateFile();
      break;
    case TESTIPC_CREATETHREAD:
      TestCreateThread();
      break;
    case TESTIPC_LAST:
      NOTREACHED_NT();
      break;
  }
  // Taken from sandbox_policy_base.cc
  size_t shared_size = base::GetPageSize() * 2;
  const size_t channel_size = sandbox::kIPCChannelSize;
  // Calculate how many channels can fit in the shared memory.
  shared_size -= offsetof(IPCControl, channels);
  size_t channel_count = shared_size / (sizeof(ChannelControl) + channel_size);
  size_t base_start =
      (sizeof(ChannelControl) * channel_count) + offsetof(IPCControl, channels);

  void* memory = GetGlobalIPCMemory();
  if (!memory)
    return SBOX_TEST_FAILED;

  // structure taken from crosscall_params.h
  struct ipc_internal {
    uint32_t tag;
    uint32_t is_in_out;
    CrossCallReturn answer;
  };

  auto* ipc_data = reinterpret_cast<ipc_internal*>(
      reinterpret_cast<char*>(memory) + base_start);

  return base::win::HandleToUint32(ipc_data->answer.handle);
}

TEST(IPCTest, IPCLeak) {
  struct TestData {
    TestId test_id;
    const char* test_name;
    HANDLE expected_result;
  } test_data[] = {{TESTIPC_NTOPENFILE, "TESTIPC_NTOPENFILE", nullptr},
                   {TESTIPC_NTCREATEFILE, "TESTIPC_NTCREATEFILE", nullptr},
                   {TESTIPC_CREATETHREAD, "TESTIPC_CREATETHREAD", nullptr}};

  static_assert(std::size(test_data) == TESTIPC_LAST, "Not enough tests.");
  for (auto test : test_data) {
    TestRunner runner;
    // There has to be a policy allocated for the child to have one to replace.
    runner.AllowFileAccess(sandbox::FileSemantics::kAllowReadonly,
                           L"c:\\Windows\\System32\\Nothing.txt");
    std::wstring command = std::wstring(L"IPC_Leak ");
    command += std::to_wstring(test.test_id);
    EXPECT_EQ(test.expected_result,
              base::win::Uint32ToHandle(runner.RunTest(command.c_str())))
        << test.test_name;
  }
}

}  // namespace sandbox
