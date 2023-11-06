// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/native_pixmap_handle_mojom_traits.h"

#include <linux/kcmp.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "base/process/process.h"
#include "base/test/gtest_util.h"
#include "media/mojo/mojom/stable/mojom_traits_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
TEST(NativePixmapHandleMojomTraitsTest, ValidNativePixmapPlane) {
  gfx::NativePixmapPlane native_pixmap_plane;
  native_pixmap_plane.stride = 50;
  native_pixmap_plane.offset = 0;
  native_pixmap_plane.size = 2500;
  native_pixmap_plane.fd = CreateValidLookingBufferHandle(
      native_pixmap_plane.offset + native_pixmap_plane.size);

  // Mojo serialization can be destructive, so we dup() the FD before
  // serialization in order to use it later to compare it against the FD in the
  // deserialized message.
  base::ScopedFD duped_fd(HANDLE_EINTR(dup(native_pixmap_plane.fd.get())));
  ASSERT_TRUE(duped_fd.is_valid());

  auto message = stable::mojom::NativePixmapPlane::SerializeAsMessage(
      &native_pixmap_plane);
  ASSERT_TRUE(!message.IsNull());

  // Required to pass base deserialize checks.
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  ASSERT_TRUE(handle.is_valid());
  auto received_message = mojo::Message::CreateFromMessageHandle(&handle);
  ASSERT_TRUE(!received_message.IsNull());

  gfx::NativePixmapPlane deserialized_native_pixmap_plane;
  ASSERT_TRUE(stable::mojom::NativePixmapPlane::DeserializeFromMessage(
      std::move(received_message), &deserialized_native_pixmap_plane));

  EXPECT_EQ(native_pixmap_plane.stride,
            deserialized_native_pixmap_plane.stride);
  EXPECT_EQ(native_pixmap_plane.offset,
            deserialized_native_pixmap_plane.offset);
  EXPECT_EQ(native_pixmap_plane.size, deserialized_native_pixmap_plane.size);
  ASSERT_TRUE(deserialized_native_pixmap_plane.fd.is_valid());
  const auto pid = base::Process::Current().Pid();
  EXPECT_EQ(syscall(SYS_kcmp, pid, pid, KCMP_FILE, duped_fd.get(),
                    deserialized_native_pixmap_plane.fd.get()),
            0);
}

TEST(NativePixmapHandleMojomTraitsTest, NativePixmapPlaneWithInvalidFd) {
  gfx::NativePixmapPlane native_pixmap_plane;
  native_pixmap_plane.stride = 50;
  native_pixmap_plane.offset = 0;
  native_pixmap_plane.size = 2500;
  native_pixmap_plane.fd = base::ScopedFD();

  ASSERT_CHECK_DEATH(stable::mojom::NativePixmapPlane::SerializeAsMessage(
      &native_pixmap_plane));
}

TEST(NativePixmapHandleMojomTraitsTest, ValidNativePixmapHandle) {
  gfx::NativePixmapHandle native_pixmap_handle;
  uint32_t stride = 50;
  uint64_t offset = 0;
  uint64_t size = 2500;
  native_pixmap_handle.planes.emplace_back(
      stride, offset, size, CreateValidLookingBufferHandle(size + offset));
  stride = 25;
  offset = 2500;
  size = 625;
  native_pixmap_handle.planes.emplace_back(
      stride, offset, size, CreateValidLookingBufferHandle(size + offset));
  native_pixmap_handle.modifier = 123u;
  native_pixmap_handle.supports_zero_copy_webgpu_import = true;

  // Mojo serialization can be destructive, so we dup() the FDs before
  // serialization in order to use it later to compare it against the FDs in the
  // deserialized message.
  std::vector<base::ScopedFD> duped_fds(native_pixmap_handle.planes.size());
  for (size_t i = 0; i < native_pixmap_handle.planes.size(); i++) {
    duped_fds[i] = base::ScopedFD(
        HANDLE_EINTR(dup(native_pixmap_handle.planes[i].fd.get())));
    ASSERT_TRUE(duped_fds[i].is_valid());
  }

  auto message = stable::mojom::NativePixmapHandle::SerializeAsMessage(
      &native_pixmap_handle);
  ASSERT_TRUE(!message.IsNull());

  // Required to pass base deserialize checks.
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  ASSERT_TRUE(handle.is_valid());
  auto received_message = mojo::Message::CreateFromMessageHandle(&handle);
  ASSERT_TRUE(!received_message.IsNull());

  gfx::NativePixmapHandle deserialized_native_pixmap_handle;
  ASSERT_TRUE(stable::mojom::NativePixmapHandle::DeserializeFromMessage(
      std::move(received_message), &deserialized_native_pixmap_handle));

  ASSERT_EQ(native_pixmap_handle.planes.size(),
            deserialized_native_pixmap_handle.planes.size());
  const auto pid = base::Process::Current().Pid();
  for (size_t i = 0; i < native_pixmap_handle.planes.size(); i++) {
    EXPECT_EQ(native_pixmap_handle.planes[i].stride,
              deserialized_native_pixmap_handle.planes[i].stride);
    EXPECT_EQ(native_pixmap_handle.planes[i].offset,
              deserialized_native_pixmap_handle.planes[i].offset);
    EXPECT_EQ(native_pixmap_handle.planes[i].size,
              deserialized_native_pixmap_handle.planes[i].size);
    ASSERT_TRUE(deserialized_native_pixmap_handle.planes[i].fd.is_valid());
    EXPECT_EQ(syscall(SYS_kcmp, pid, pid, KCMP_FILE, duped_fds[i].get(),
                      deserialized_native_pixmap_handle.planes[i].fd.get()),
              0);
  }
  EXPECT_EQ(native_pixmap_handle.modifier,
            deserialized_native_pixmap_handle.modifier);
  // The |supports_zero_copy_webgpu_import| field is not intended to cross
  // process boundaries. It will not be serialized and it is set to false by
  // default.
  EXPECT_FALSE(
      deserialized_native_pixmap_handle.supports_zero_copy_webgpu_import);
}

TEST(NativePixmapHandleMojomTraitsTest, NativePixmapHandleWithInvalidFd) {
  gfx::NativePixmapHandle native_pixmap_handle;
  uint32_t stride = 50;
  uint64_t offset = 0;
  uint64_t size = 2500;
  native_pixmap_handle.planes.emplace_back(
      stride, offset, size, CreateValidLookingBufferHandle(size + offset));
  stride = 25;
  offset = 2500;
  size = 625;
  native_pixmap_handle.planes.emplace_back(stride, offset, size,
                                           base::ScopedFD());
  native_pixmap_handle.modifier = 123u;
  native_pixmap_handle.supports_zero_copy_webgpu_import = true;

  ASSERT_CHECK_DEATH(stable::mojom::NativePixmapHandle::SerializeAsMessage(
      &native_pixmap_handle));
}
}  // namespace media
