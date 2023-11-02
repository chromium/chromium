// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_handle.h"

#include <tuple>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include <mach/mach_vm.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#else
#include "base/files/scoped_file.h"
#endif

namespace mojo {
namespace {

// Different types of platform handles are supported on different platforms.
// We run all PlatformHandle once for each type of handle available on the
// target platform.
enum class HandleType {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
  kHandle,
#endif
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  kFileDescriptor,
#endif
#if BUILDFLAG(IS_MAC)
  kMachPort,
#endif
};

// Different types of test modes we support in order to exercise the available
// handles types. Fuchsia zx_handle tests and Mac Mach port tests use shared
// memory for setup and validation. Everything else uses platform files.
// See |SetUp()| below.
enum class TestType {
  kFile,
  kSharedMemory,
};

const std::string kTestData = "some data to validate";

class PlatformHandleTest : public testing::Test,
                           public testing::WithParamInterface<HandleType> {
 public:
  PlatformHandleTest() = default;
  PlatformHandleTest(const PlatformHandleTest&) = delete;
  PlatformHandleTest& operator=(const PlatformHandleTest&) = delete;

  void SetUp() override {
    test_type_ = TestType::kFile;

#if BUILDFLAG(IS_FUCHSIA)
    if (GetParam() == HandleType::kHandle)
      test_type_ = TestType::kSharedMemory;
#elif BUILDFLAG(IS_MAC)
    if (GetParam() == HandleType::kMachPort)
      test_type_ = TestType::kSharedMemory;
#endif

    if (test_type_ == TestType::kFile)
      test_handle_ = SetUpFile();
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC)
    else
      test_handle_ = SetUpSharedMemory();
#endif
  }

  // Extracts the contents of a file or shared memory object, given a generic
  // PlatformHandle wrapping it. Used to verify that a |handle| refers to some
  // expected platform object.
  std::string GetObjectContents(PlatformHandle& handle) {
    if (test_type_ == TestType::kFile)
      return GetFileContents(handle);
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC)
    return GetSharedMemoryContents(handle);
#else
    NOTREACHED();
    return std::string();
#endif
  }

 protected:
  PlatformHandle& test_handle() { return test_handle_; }

 private:
  // Creates a platform file with some test data in it. Leaves the file open
  // with cursor positioned at the beginning, and returns it as a generic
  // PlatformHandle.
  PlatformHandle SetUpFile() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::File test_file(temp_dir_.GetPath().AppendASCII("test"),
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_READ);
    test_file.WriteAtCurrentPos(kTestData.data(),
                                static_cast<int>(kTestData.size()));

#if BUILDFLAG(IS_WIN)
    return PlatformHandle(
        base::win::ScopedHandle(test_file.TakePlatformFile()));
#else
    return PlatformHandle(base::ScopedFD(test_file.TakePlatformFile()));
#endif
  }

  // Returns the contents of a platform file referenced by |handle|. Used to
  // verify that |handle| is in fact the platform file handle it's expected to
  // be. See |GetObjectContents()|.
  std::string GetFileContents(PlatformHandle& handle) {
#if BUILDFLAG(IS_WIN)
    // We must temporarily release ownership of the handle due to how File
    // interacts with ScopedHandle.
    base::File file(handle.TakeHandle());
#else
    // Do the same as Windows for consistency, even though it is not necessary.
    base::File file(handle.TakeFD());
#endif
    std::vector<char> buffer(kTestData.size());
    file.Read(0, buffer.data(), static_cast<int>(buffer.size()));
    std::string contents(buffer.begin(), buffer.end());

// Let |handle| retain ownership.
#if BUILDFLAG(IS_WIN)
    handle = PlatformHandle(base::win::ScopedHandle(file.TakePlatformFile()));
#else
    handle = PlatformHandle(base::ScopedFD(file.TakePlatformFile()));
#endif

    return contents;
  }

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC)
  // Creates a shared memory region with some test data in it. Leaves the
  // handle open and returns it as a generic PlatformHandle.
  PlatformHandle SetUpSharedMemory() {
    auto region = base::UnsafeSharedMemoryRegion::Create(kTestData.size());
    auto mapping = region.Map();
    memcpy(mapping.memory(), kTestData.data(), kTestData.size());
    auto generic_region =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(region));
    shm_guid_ = generic_region.GetGUID();
    return PlatformHandle(generic_region.PassPlatformHandle());
  }

  // Extracts data stored in a shared memory object referenced by |handle|. Used
  // to verify that |handle| does in fact reference a shared memory object when
  // expected. See |GetObjectContents()|.
  std::string GetSharedMemoryContents(const PlatformHandle& handle) {
    base::subtle::ScopedPlatformSharedMemoryHandle region_handle(
#if BUILDFLAG(IS_FUCHSIA)
        handle.GetHandle().get()
#elif BUILDFLAG(IS_MAC)
        handle.GetMachSendRight().get()
#endif
    );
    auto generic_region = base::subtle::PlatformSharedMemoryRegion::Take(
        std::move(region_handle),
        base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
        kTestData.size(), shm_guid_);
    auto region =
        base::UnsafeSharedMemoryRegion::Deserialize(std::move(generic_region));
    auto mapping = region.Map();
    std::string contents(static_cast<char*>(mapping.memory()),
                         kTestData.size());

    // Let |handle| retain ownership.
    generic_region = base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
        std::move(region));
    region_handle = generic_region.PassPlatformHandle();
    std::ignore = region_handle.release();

    return contents;
  }
#endif  // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC)

  base::ScopedTempDir temp_dir_;
  TestType test_type_;
  PlatformHandle test_handle_;

  // Needed to reconstitute a base::PlatformSharedMemoryRegion from an unwrapped
  // PlatformHandle.
  base::UnguessableToken shm_guid_;
};

TEST_P(PlatformHandleTest, BasicConstruction) {
  EXPECT_EQ(kTestData, GetObjectContents(test_handle()));
}

TEST_P(PlatformHandleTest, Move) {
  EXPECT_EQ(kTestData, GetObjectContents(test_handle()));

  auto new_handle = std::move(test_handle());
  EXPECT_FALSE(test_handle().is_valid());
  EXPECT_TRUE(new_handle.is_valid());

  EXPECT_EQ(kTestData, GetObjectContents(new_handle));
}

TEST_P(PlatformHandleTest, Reset) {
  auto handle = std::move(test_handle());
  EXPECT_TRUE(handle.is_valid());
  handle.reset();
  EXPECT_FALSE(handle.is_valid());
}

TEST_P(PlatformHandleTest, Clone) {
  EXPECT_EQ(kTestData, GetObjectContents(test_handle()));

  auto clone = test_handle().Clone();
  EXPECT_TRUE(clone.is_valid());
  EXPECT_EQ(kTestData, GetObjectContents(clone));
  clone.reset();
  EXPECT_FALSE(clone.is_valid());

  EXPECT_TRUE(test_handle().is_valid());
  EXPECT_EQ(kTestData, GetObjectContents(test_handle()));
}

// This is really testing system library stuff, but we conveniently have all
// this handle type parameterization already in place here.
TEST_P(PlatformHandleTest, CStructConversion) {
  EXPECT_EQ(kTestData, GetObjectContents(test_handle()));

  MojoPlatformHandle c_handle;
  PlatformHandle::ToMojoPlatformHandle(std::move(test_handle()), &c_handle);

  PlatformHandle handle = PlatformHandle::FromMojoPlatformHandle(&c_handle);
  EXPECT_EQ(kTestData, GetObjectContents(handle));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PlatformHandleTest,
#if BUILDFLAG(IS_WIN)
                         testing::Values(HandleType::kHandle)
#elif BUILDFLAG(IS_FUCHSIA)
                         testing::Values(HandleType::kHandle,
                                         HandleType::kFileDescriptor)
#elif BUILDFLAG(IS_MAC)
                         testing::Values(HandleType::kFileDescriptor,
                                         HandleType::kMachPort)
#elif BUILDFLAG(IS_POSIX)
                         testing::Values(HandleType::kFileDescriptor)
#endif
);

}  // namespace
}  // namespace mojo
