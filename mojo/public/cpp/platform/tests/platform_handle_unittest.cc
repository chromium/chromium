// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_handle.h"

#include <tuple>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "base/android/binder.h"
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
    test_file.WriteAtCurrentPos(base::as_byte_span(kTestData));

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
    file.Read(0, base::as_writable_byte_span(buffer));
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
    base::as_writable_chars(base::span(mapping)).copy_from(kTestData);
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
    std::string contents(base::as_string_view(mapping));

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

#if BUILDFLAG(IS_ANDROID)
DEFINE_BINDER_CLASS(IncrementerInterface);
class Incrementer : public base::android::SupportsBinder<IncrementerInterface> {
 public:
  using Proxy = IncrementerInterface::BinderRef;

  Incrementer() = default;

  static constexpr transaction_code_t kDoIncrement = 42;

  static int32_t DoIncrement(Proxy& proxy, int32_t value) {
    const auto reply = *proxy.Transact(
        kDoIncrement,
        [value](const auto& parcel) { return parcel.WriteInt32(value); });
    return *reply.reader().ReadInt32();
  }

  void WaitForDisconnect() { disconnect_.Wait(); }

 private:
  ~Incrementer() override = default;

  // base::android::SupportsBinder<IncrementerInterface>
  base::android::BinderStatusOr<void> OnBinderTransaction(
      transaction_code_t code,
      const base::android::ParcelReader& in,
      const base::android::ParcelWriter& out) override {
    EXPECT_EQ(kDoIncrement, code);
    return out.WriteInt32(*in.ReadInt32() + 1);
  }

  void OnBinderDestroyed() override { disconnect_.Signal(); }

  base::WaitableEvent disconnect_;
};

TEST(PlatformHandleBinderTest, HandleTypeAndValidity) {
  if (!base::android::IsNativeBinderAvailable()) {
    GTEST_SKIP() << "This test is only valid with native Binder support (Q+)";
  }

  auto handle = PlatformHandle(base::android::BinderRef());
  EXPECT_TRUE(handle.is_binder());
  EXPECT_FALSE(handle.is_valid_binder());
  EXPECT_FALSE(handle.is_fd());
  EXPECT_FALSE(handle.is_valid_fd());
  EXPECT_FALSE(handle.is_valid());

  auto incrementer = base::MakeRefCounted<Incrementer>();
  handle = PlatformHandle(incrementer->GetBinder());
  EXPECT_TRUE(handle.is_binder());
  EXPECT_TRUE(handle.is_valid_binder());
  EXPECT_FALSE(handle.is_fd());
  EXPECT_FALSE(handle.is_valid_fd());
  EXPECT_TRUE(handle.is_valid());

  auto proxy = Incrementer::Proxy(handle.GetBinder());
  EXPECT_EQ(3, Incrementer::DoIncrement(proxy, 2));

  PlatformHandle other = std::move(handle);
  EXPECT_FALSE(handle.is_binder());
  EXPECT_FALSE(handle.is_valid_binder());
  EXPECT_FALSE(handle.is_valid());
  EXPECT_FALSE(handle.GetBinder());
  EXPECT_FALSE(handle.TakeBinder());
  EXPECT_TRUE(other.is_binder());
  EXPECT_TRUE(other.is_valid_binder());
  EXPECT_TRUE(other.is_valid());
  EXPECT_TRUE(other.GetBinder());

  auto binder = other.TakeBinder();
  EXPECT_TRUE(binder);
  EXPECT_FALSE(other.is_binder());
  EXPECT_FALSE(other.is_valid());

  proxy = Incrementer::Proxy(binder);
  EXPECT_EQ(7, Incrementer::DoIncrement(proxy, 6));
  proxy.reset();
  binder.reset();

  // Ensure we haven't leaked any binder refs through all this.
  incrementer->WaitForDisconnect();
}

TEST(PlatformHandleBinderTest, WrapUnwrap) {
  if (!base::android::IsNativeBinderAvailable()) {
    GTEST_SKIP() << "This test is only valid with native Binder support (Q+)";
  }

  Incrementer::Proxy proxy;
  auto handle = WrapPlatformHandle(PlatformHandle(std::move(proxy)));
  EXPECT_FALSE(handle.is_valid());

  auto incrementer = base::MakeRefCounted<Incrementer>();
  proxy = Incrementer::Proxy(incrementer->GetBinder());
  EXPECT_EQ(43, Incrementer::DoIncrement(proxy, 42));

  handle = WrapPlatformHandle(PlatformHandle(std::move(proxy)));
  EXPECT_TRUE(handle.is_valid());
  proxy =
      Incrementer::Proxy(UnwrapPlatformHandle(std::move(handle)).TakeBinder());
  EXPECT_EQ(9001, Incrementer::DoIncrement(proxy, 9000));
}
#endif

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
