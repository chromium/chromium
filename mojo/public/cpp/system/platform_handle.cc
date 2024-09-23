// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/platform_handle.h"

#include "base/check_op.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_handle_internal.h"

namespace mojo {

namespace {

uint64_t ReleasePlatformHandleValueFromPlatformFile(
    base::ScopedPlatformFile file) {
#if BUILDFLAG(IS_WIN)
  return reinterpret_cast<uint64_t>(file.Take());
#else
  return static_cast<uint64_t>(file.release());
#endif
}

base::ScopedPlatformFile PlatformFileFromPlatformHandleValue(uint64_t value) {
#if BUILDFLAG(IS_WIN)
  return base::ScopedPlatformFile(reinterpret_cast<base::PlatformFile>(value));
#else
  return base::ScopedPlatformFile(static_cast<base::PlatformFile>(value));
#endif
}

}  // namespace

ScopedSharedBufferHandle WrapPlatformSharedMemoryRegion(
    base::subtle::PlatformSharedMemoryRegion region) {
  if (!region.IsValid())
    return ScopedSharedBufferHandle();

  MojoPlatformSharedMemoryRegionAccessMode access_mode;
  switch (region.GetMode()) {
    case base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly:
      access_mode = MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY;
      break;
    case base::subtle::PlatformSharedMemoryRegion::Mode::kWritable:
      access_mode = MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE;
      break;
    case base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe:
      access_mode = MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_UNSAFE;
      break;
    default:
      NOTREACHED();
  }

  base::subtle::ScopedPlatformSharedMemoryHandle handle =
      region.PassPlatformHandle();
  MojoPlatformHandle platform_handles[2];
  uint32_t num_platform_handles = 1;
  platform_handles[0].struct_size = sizeof(platform_handles[0]);
#if BUILDFLAG(IS_WIN)
  platform_handles[0].type = MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;
  platform_handles[0].value = reinterpret_cast<uint64_t>(handle.Take());
#elif BUILDFLAG(IS_FUCHSIA)
  platform_handles[0].type = MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE;
  platform_handles[0].value = static_cast<uint64_t>(handle.release());
#elif BUILDFLAG(IS_APPLE)
  platform_handles[0].type = MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT;
  platform_handles[0].value = static_cast<uint64_t>(handle.release());
#elif BUILDFLAG(IS_ANDROID)
  platform_handles[0].type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
  platform_handles[0].value = static_cast<uint64_t>(handle.release());
#else
  platform_handles[0].type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
  platform_handles[0].value = static_cast<uint64_t>(handle.fd.release());

  if (region.GetMode() ==
      base::subtle::PlatformSharedMemoryRegion::Mode::kWritable) {
    num_platform_handles = 2;
    platform_handles[1].struct_size = sizeof(platform_handles[1]);
    platform_handles[1].type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
    platform_handles[1].value =
        static_cast<uint64_t>(handle.readonly_fd.release());
  }
#endif
  MojoSharedBufferGuid mojo_guid =
      mojo::internal::PlatformHandleInternal::MarshalUnguessableToken(
          region.GetGUID());
  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformSharedMemoryRegion(
      platform_handles, num_platform_handles, region.GetSize(), &mojo_guid,
      access_mode, nullptr, &mojo_handle);
  if (result != MOJO_RESULT_OK)
    return ScopedSharedBufferHandle();
  return ScopedSharedBufferHandle(SharedBufferHandle(mojo_handle));
}

base::subtle::PlatformSharedMemoryRegion UnwrapPlatformSharedMemoryRegion(
    ScopedSharedBufferHandle mojo_handle) {
  if (!mojo_handle.is_valid())
    return base::subtle::PlatformSharedMemoryRegion();

  MojoPlatformHandle platform_handles[2];
  platform_handles[0].struct_size = sizeof(platform_handles[0]);
  platform_handles[1].struct_size = sizeof(platform_handles[1]);
  uint32_t num_platform_handles = 2;
  uint64_t size;
  MojoSharedBufferGuid mojo_guid;
  MojoPlatformSharedMemoryRegionAccessMode access_mode;
  MojoResult result = MojoUnwrapPlatformSharedMemoryRegion(
      mojo_handle.release().value(), nullptr, platform_handles,
      &num_platform_handles, &size, &mojo_guid, &access_mode);
  if (result != MOJO_RESULT_OK)
    return base::subtle::PlatformSharedMemoryRegion();

  base::subtle::ScopedPlatformSharedMemoryHandle region_handle;
#if BUILDFLAG(IS_WIN)
  if (num_platform_handles != 1)
    return base::subtle::PlatformSharedMemoryRegion();
  if (platform_handles[0].type != MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.Set(reinterpret_cast<HANDLE>(platform_handles[0].value));
#elif BUILDFLAG(IS_FUCHSIA)
  if (num_platform_handles != 1)
    return base::subtle::PlatformSharedMemoryRegion();
  if (platform_handles[0].type != MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.reset(static_cast<zx_handle_t>(platform_handles[0].value));
#elif BUILDFLAG(IS_APPLE)
  if (num_platform_handles != 1)
    return base::subtle::PlatformSharedMemoryRegion();
  if (platform_handles[0].type != MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.reset(static_cast<mach_port_t>(platform_handles[0].value));
#elif BUILDFLAG(IS_ANDROID)
  if (num_platform_handles != 1)
    return base::subtle::PlatformSharedMemoryRegion();
  if (platform_handles[0].type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.reset(static_cast<int>(platform_handles[0].value));
#else
  if (access_mode == MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE) {
    if (num_platform_handles != 2)
      return base::subtle::PlatformSharedMemoryRegion();
  } else if (num_platform_handles != 1) {
    return base::subtle::PlatformSharedMemoryRegion();
  }
  if (platform_handles[0].type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.fd.reset(static_cast<int>(platform_handles[0].value));
  if (num_platform_handles == 2) {
    if (platform_handles[1].type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
      return base::subtle::PlatformSharedMemoryRegion();
    region_handle.readonly_fd.reset(
        static_cast<int>(platform_handles[1].value));
  }
#endif

  base::subtle::PlatformSharedMemoryRegion::Mode mode;
  switch (access_mode) {
    case MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY:
      mode = base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly;
      break;
    case MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE:
      mode = base::subtle::PlatformSharedMemoryRegion::Mode::kWritable;
      break;
    case MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_UNSAFE:
      mode = base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
      break;
    default:
      return base::subtle::PlatformSharedMemoryRegion();
  }

  std::optional<base::UnguessableToken> guid =
      internal::PlatformHandleInternal::UnmarshalUnguessableToken(&mojo_guid);
  if (!guid.has_value()) {
    return base::subtle::PlatformSharedMemoryRegion();
  }

  return base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(region_handle), mode, size, guid.value());
}

ScopedHandle WrapPlatformHandle(PlatformHandle handle) {
  if (!handle.is_valid()) {
    return ScopedHandle();
  }

  MojoPlatformHandle platform_handle;
  PlatformHandle::ToMojoPlatformHandle(std::move(handle), &platform_handle);

  MojoHandle wrapped_handle;
  MojoResult result =
      MojoWrapPlatformHandle(&platform_handle, nullptr, &wrapped_handle);
  if (result != MOJO_RESULT_OK)
    return ScopedHandle();
  return ScopedHandle(Handle(wrapped_handle));
}

PlatformHandle UnwrapPlatformHandle(ScopedHandle handle) {
  if (!handle.is_valid()) {
    return PlatformHandle();
  }

  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  MojoResult result = MojoUnwrapPlatformHandle(handle.release().value(),
                                               nullptr, &platform_handle);
  if (result != MOJO_RESULT_OK)
    return PlatformHandle();
  return PlatformHandle::FromMojoPlatformHandle(&platform_handle);
}

ScopedHandle WrapPlatformFile(base::ScopedPlatformFile platform_file) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  platform_handle.type = kPlatformFileHandleType;
  platform_handle.value =
      ReleasePlatformHandleValueFromPlatformFile(std::move(platform_file));

  MojoHandle mojo_handle;
  MojoResult result =
      MojoWrapPlatformHandle(&platform_handle, nullptr, &mojo_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return ScopedHandle(Handle(mojo_handle));
}

MojoResult UnwrapPlatformFile(ScopedHandle handle,
                              base::ScopedPlatformFile* file) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  MojoResult result = MojoUnwrapPlatformHandle(handle.release().value(),
                                               nullptr, &platform_handle);
  if (result != MOJO_RESULT_OK)
    return result;

  if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_INVALID) {
    *file = base::ScopedPlatformFile();
  } else {
    CHECK_EQ(platform_handle.type, kPlatformFileHandleType);
    *file = PlatformFileFromPlatformHandleValue(platform_handle.value);
  }

  return MOJO_RESULT_OK;
}

ScopedSharedBufferHandle WrapReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion region) {
  return WrapPlatformSharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)));
}

ScopedSharedBufferHandle WrapUnsafeSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion region) {
  return WrapPlatformSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)));
}

ScopedSharedBufferHandle WrapWritableSharedMemoryRegion(
    base::WritableSharedMemoryRegion region) {
  return WrapPlatformSharedMemoryRegion(
      base::WritableSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)));
}

base::ReadOnlySharedMemoryRegion UnwrapReadOnlySharedMemoryRegion(
    ScopedSharedBufferHandle handle) {
  return base::ReadOnlySharedMemoryRegion::Deserialize(
      UnwrapPlatformSharedMemoryRegion(std::move(handle)));
}

base::UnsafeSharedMemoryRegion UnwrapUnsafeSharedMemoryRegion(
    ScopedSharedBufferHandle handle) {
  return base::UnsafeSharedMemoryRegion::Deserialize(
      UnwrapPlatformSharedMemoryRegion(std::move(handle)));
}

base::WritableSharedMemoryRegion UnwrapWritableSharedMemoryRegion(
    ScopedSharedBufferHandle handle) {
  return base::WritableSharedMemoryRegion::Deserialize(
      UnwrapPlatformSharedMemoryRegion(std::move(handle)));
}

}  // namespace mojo
