// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_nt_util.h"

#include <windows.h>

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/win_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

TEST(SandboxNtUtil, IsSameProcessPseudoHandle) {
  InitGlobalNt();

  HANDLE current_process_pseudo = GetCurrentProcess();
  EXPECT_TRUE(IsSameProcess(current_process_pseudo));
}

TEST(SandboxNtUtil, IsSameProcessNonPseudoHandle) {
  InitGlobalNt();

  base::win::ScopedHandle current_process(
      OpenProcess(PROCESS_QUERY_INFORMATION, false, GetCurrentProcessId()));
  ASSERT_TRUE(current_process.IsValid());
  EXPECT_TRUE(IsSameProcess(current_process.Get()));
}

TEST(SandboxNtUtil, IsSameProcessDifferentProcess) {
  InitGlobalNt();

  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {};
  wchar_t notepad[] = L"notepad";
  ASSERT_TRUE(CreateProcessW(nullptr, notepad, nullptr, nullptr, false, 0,
                             nullptr, nullptr, &si, &pi));
  base::win::ScopedProcessInformation process_info(pi);

  EXPECT_FALSE(IsSameProcess(process_info.process_handle()));
  EXPECT_TRUE(TerminateProcess(process_info.process_handle(), 0));
}

struct VirtualMemDeleter {
  void operator()(char* p) { ::VirtualFree(p, 0, MEM_RELEASE); }
};

typedef std::unique_ptr<char, VirtualMemDeleter> unique_ptr_vmem;

#if defined(_WIN64)

void AllocateBlock(SIZE_T size,
                   SIZE_T free_size,
                   char** base_address,
                   std::vector<unique_ptr_vmem>* mem_range) {
  unique_ptr_vmem ptr(static_cast<char*>(::VirtualAlloc(
      *base_address, size - free_size, MEM_RESERVE, PAGE_READWRITE)));
  ASSERT_NE(nullptr, ptr.get());
  mem_range->push_back(std::move(ptr));
  *base_address += size;
}

#define KIB(x) ((x)*1024ULL)
#define MIB(x) (KIB(x) * 1024ULL)
#define GIB(x) (MIB(x) * 1024ULL)
// Construct a basic memory layout to do the test. We reserve first to get a
// base address then reallocate with the following pattern.
// |512MiB-64KiB Free|512MiB-128Kib Free|512MiB-256Kib Free|512MiB+512KiB Free|
// The purpose of this is leave a couple of free memory regions within a 2GiB
// block of reserved memory that we can test the searching allocator.
void AllocateTestRange(std::vector<unique_ptr_vmem>* mem_range) {
  // Ensure we preallocate enough space in the vector to prevent unexpected
  // allocations.
  mem_range->reserve(5);
  SIZE_T total_size =
      MIB(512) + MIB(512) + MIB(512) + MIB(512) + KIB(512) + KIB(64);
  unique_ptr_vmem ptr(static_cast<char*>(
      ::VirtualAlloc(nullptr, total_size, MEM_RESERVE, PAGE_READWRITE)));
  ASSERT_NE(nullptr, ptr.get());
  char* base_address = ptr.get();
  char* orig_base = base_address;
  ptr.reset();
  AllocateBlock(MIB(512), KIB(64), &base_address, mem_range);
  AllocateBlock(MIB(512), KIB(128), &base_address, mem_range);
  AllocateBlock(MIB(512), KIB(256), &base_address, mem_range);
  AllocateBlock(MIB(512) + KIB(512), KIB(512), &base_address, mem_range);
  // Allocate a memory block at end to act as an upper bound.
  AllocateBlock(KIB(64), 0, &base_address, mem_range);
  ASSERT_EQ(total_size, static_cast<SIZE_T>(base_address - orig_base));
}

// Test we can allocate appropriate blocks.
void TestAlignedRange(char* base_address) {
  unique_ptr_vmem ptr_256k(new (sandbox::NT_PAGE, base_address) char[KIB(256)]);
  EXPECT_EQ(base_address + GIB(1) + MIB(512) - KIB(256), ptr_256k.get());
  unique_ptr_vmem ptr_64k(new (sandbox::NT_PAGE, base_address) char[KIB(64)]);
  EXPECT_EQ(base_address + MIB(512) - KIB(64), ptr_64k.get());
  unique_ptr_vmem ptr_128k(new (sandbox::NT_PAGE, base_address) char[KIB(128)]);
  EXPECT_EQ(base_address + GIB(1) - KIB(128), ptr_128k.get());
  // We will have run out of space here so should also fail.
  unique_ptr_vmem ptr_64k_noalloc(
      new (sandbox::NT_PAGE, base_address) char[KIB(64)]);
  EXPECT_EQ(nullptr, ptr_64k_noalloc.get());
}

// Test the 512k block which exists at the end of the maximum allocation
// boundary.
void Test512kBlock(char* base_address) {
  // This should fail as it'll just be out of range.
  unique_ptr_vmem ptr_512k_noalloc(
      new (sandbox::NT_PAGE, base_address) char[KIB(512)]);
  EXPECT_EQ(nullptr, ptr_512k_noalloc.get());
  // Check that moving base address we can allocate the 512k block.
  unique_ptr_vmem ptr_512k(
      new (sandbox::NT_PAGE, base_address + GIB(1)) char[KIB(512)]);
  EXPECT_EQ(base_address + GIB(2), ptr_512k.get());
  // Free pointer first.
  ptr_512k.reset();
  ptr_512k.reset(new (sandbox::NT_PAGE, base_address + GIB(2)) char[KIB(512)]);
  EXPECT_EQ(base_address + GIB(2), ptr_512k.get());
}

// Test we can allocate appropriate blocks even when starting at an unaligned
// address.
void TestUnalignedRange(char* base_address) {
  char* unaligned_base = base_address + 123456;
  unique_ptr_vmem ptr_256k(
      new (sandbox::NT_PAGE, unaligned_base) char[KIB(256)]);
  EXPECT_EQ(base_address + GIB(1) + MIB(512) - KIB(256), ptr_256k.get());
  unique_ptr_vmem ptr_64k(new (sandbox::NT_PAGE, unaligned_base) char[KIB(64)]);
  EXPECT_EQ(base_address + MIB(512) - KIB(64), ptr_64k.get());
  unique_ptr_vmem ptr_128k(
      new (sandbox::NT_PAGE, unaligned_base) char[KIB(128)]);
  EXPECT_EQ(base_address + GIB(1) - KIB(128), ptr_128k.get());
}

// Test maximum number of available allocations within the predefined pattern.
void TestMaxAllocations(char* base_address) {
  // There's only 7 64k blocks in the first 2g which we can fill.
  unique_ptr_vmem ptr_1(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_NE(nullptr, ptr_1.get());
  unique_ptr_vmem ptr_2(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_NE(nullptr, ptr_2.get());
  unique_ptr_vmem ptr_3(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_NE(nullptr, ptr_3.get());
  unique_ptr_vmem ptr_4(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_NE(nullptr, ptr_4.get());
  unique_ptr_vmem ptr_5(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_NE(nullptr, ptr_5.get());
  unique_ptr_vmem ptr_6(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_NE(nullptr, ptr_6.get());
  unique_ptr_vmem ptr_7(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_NE(nullptr, ptr_7.get());
  unique_ptr_vmem ptr_8(new (sandbox::NT_PAGE, base_address) char[1]);
  EXPECT_EQ(nullptr, ptr_8.get());
}

// Test extreme allocations we know should fail.
void TestExtremes() {
  unique_ptr_vmem ptr_null(new (sandbox::NT_PAGE, nullptr) char[1]);
  EXPECT_EQ(nullptr, ptr_null.get());
  unique_ptr_vmem ptr_too_large(
      new (sandbox::NT_PAGE, reinterpret_cast<void*>(0x1000000)) char[GIB(4)]);
  EXPECT_EQ(nullptr, ptr_too_large.get());
  unique_ptr_vmem ptr_overflow(
      new (sandbox::NT_PAGE, reinterpret_cast<void*>(SIZE_MAX)) char[1]);
  EXPECT_EQ(nullptr, ptr_overflow.get());
  unique_ptr_vmem ptr_invalid(new (
      sandbox::NT_PAGE, reinterpret_cast<void*>(SIZE_MAX - 0x1000000)) char[1]);
  EXPECT_EQ(nullptr, ptr_invalid.get());
}

// Test nearest allocator, only do this for 64 bit. We test through the exposed
// new operator as we can't call the AllocateNearTo function directly.
TEST(SandboxNtUtil, NearestAllocator) {
  InitGlobalNt();
  std::vector<unique_ptr_vmem> mem_range;
  AllocateTestRange(&mem_range);
  ASSERT_LT(0U, mem_range.size());
  char* base_address = static_cast<char*>(mem_range[0].get());

  TestAlignedRange(base_address);
  Test512kBlock(base_address);
  TestUnalignedRange(base_address);
  TestMaxAllocations(base_address);
  TestExtremes();
}

#endif  // defined(_WIN64)

// Test whether function ValidParameter works as expected, that is properly
// checks access to the buffer and doesn't modify it in any way.
TEST(SandboxNtUtil, ValidParameter) {
  static constexpr unsigned int buffer_size = 4096;
  unique_ptr_vmem buffer_guard(static_cast<char*>(
      ::VirtualAlloc(nullptr, buffer_size, MEM_COMMIT, PAGE_READWRITE)));
  ASSERT_NE(nullptr, buffer_guard.get());

  unsigned char* ptr = reinterpret_cast<unsigned char*>(buffer_guard.get());

  // Fill the buffer with some data.
  for (unsigned int i = 0; i < buffer_size; i++)
    ptr[i] = (i % 256);

  // Setup verify function.
  auto verify_buffer = [&]() {
    for (unsigned int i = 0; i < buffer_size; i++) {
      if (ptr[i] != (i % 256))
        return false;
    }

    return true;
  };

  // Verify that the buffer can be written to and doesn't change.
  EXPECT_TRUE(ValidParameter(ptr, buffer_size, RequiredAccess::WRITE));
  EXPECT_TRUE(verify_buffer());

  DWORD old_protection;
  // Change the protection of buffer to READONLY.
  EXPECT_TRUE(
      ::VirtualProtect(ptr, buffer_size, PAGE_READONLY, &old_protection));

  // Writting to buffer should fail now.
  EXPECT_FALSE(ValidParameter(ptr, buffer_size, RequiredAccess::WRITE));

  // But reading should be ok.
  EXPECT_TRUE(ValidParameter(ptr, buffer_size, RequiredAccess::READ));

  // One final check that the buffer hasn't been modified.
  EXPECT_TRUE(verify_buffer());
}

TEST(SandboxNtUtil, NtGetPathFromHandle) {
  InitGlobalNt();

  base::FilePath exe;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &exe));
  base::File exe_file(exe, base::File::FLAG_OPEN);
  ASSERT_TRUE(exe_file.IsValid());
  std::unique_ptr<wchar_t, NtAllocDeleter> path;
  EXPECT_TRUE(NtGetPathFromHandle(exe_file.GetPlatformFile(), &path));

  // Basic sanity test, the functionality of NtGetPathFromHandle to return
  // the correct value is already tested from win_utils_unittest.cc.
  EXPECT_TRUE(base::EndsWith(base::AsStringPiece16(path.get()),
                             base::AsStringPiece16(exe.BaseName().value()),
                             base::CompareCase::INSENSITIVE_ASCII));

  // Compare to GetNtPathFromWin32Path for extra check.
  std::wstring nt_path;
  EXPECT_TRUE(GetNtPathFromWin32Path(exe.value(), &nt_path));
  EXPECT_STREQ(path.get(), nt_path.c_str());
}

}  // namespace
}  // namespace sandbox
