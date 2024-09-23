// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ipcz_driver/shared_buffer.h"

#include <cstdint>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

// Enumeration of supported region access modes.
enum class BufferMode : uint32_t {
  kReadOnly,
  kWritable,
  kUnsafe,
};

// The wire representation of a serialized shared buffer.
struct IPCZ_ALIGN(8) BufferHeader {
  // The size of this structure, in bytes. Used for versioning.
  uint32_t size;

  // The size of the shared memory buffer.
  uint32_t buffer_size;

  // Access mode for the region.
  BufferMode mode;

  // Explicit padding for the next field to be 8-byte-aligned.
  uint32_t padding;

  // The low and high components of the 128-bit GUID used to identify this
  // buffer.
  uint64_t guid_low;
  uint64_t guid_high;
};
static_assert(sizeof(BufferHeader) == 32, "Invalid BufferHeader size");

// Produces a ScopedPlatformSharedMemoryHandle from a set of PlatformHandles and
// an access mode.
base::subtle::ScopedPlatformSharedMemoryHandle
CreateRegionHandleFromPlatformHandles(
    base::span<PlatformHandle> handles,
    base::subtle::PlatformSharedMemoryRegion::Mode mode) {
  if (handles.empty()) {
    return {};
  }

#if BUILDFLAG(IS_WIN)
  return handles[0].TakeHandle();
#elif BUILDFLAG(IS_FUCHSIA)
  return zx::vmo(handles[0].TakeHandle());
#elif BUILDFLAG(IS_APPLE)
  return handles[0].TakeMachSendRight();
#elif BUILDFLAG(IS_ANDROID)
  return handles[0].TakeFD();
#else
  base::ScopedFD readonly_fd;
  if (mode == base::subtle::PlatformSharedMemoryRegion::Mode::kWritable) {
    if (handles.size() < 2) {
      return {};
    }
    readonly_fd = handles[1].TakeFD();
  }
  base::ScopedFD fd = handles[0].TakeFD();
  return base::subtle::ScopedFDPair(std::move(fd), std::move(readonly_fd));
#endif
}

}  // namespace

SharedBuffer::SharedBuffer(base::subtle::PlatformSharedMemoryRegion region)
    : region_(std::move(region)) {}

SharedBuffer::~SharedBuffer() = default;

std::pair<scoped_refptr<SharedBuffer>, IpczResult> SharedBuffer::Duplicate(
    bool read_only) {
  using Mode = base::subtle::PlatformSharedMemoryRegion::Mode;
  if (region_.GetMode() == Mode::kWritable) {
    if (read_only && !region_.ConvertToReadOnly()) {
      return {nullptr, MOJO_RESULT_RESOURCE_EXHAUSTED};
    } else if (!read_only && !region_.ConvertToUnsafe()) {
      return {nullptr, MOJO_RESULT_RESOURCE_EXHAUSTED};
    }
  }

  const Mode required_mode = read_only ? Mode::kReadOnly : Mode::kUnsafe;
  if (region_.GetMode() != required_mode) {
    return {nullptr, MOJO_RESULT_FAILED_PRECONDITION};
  }

  auto new_region = region_.Duplicate();
  if (!new_region.IsValid()) {
    return {nullptr, MOJO_RESULT_RESOURCE_EXHAUSTED};
  }

  return {
      base::MakeRefCounted<ipcz_driver::SharedBuffer>(std::move(new_region)),
      IPCZ_RESULT_OK};
}

// static
scoped_refptr<SharedBuffer> SharedBuffer::CreateForMojoWrapper(
    base::span<const MojoPlatformHandle> mojo_platform_handles,
    uint32_t size,
    const MojoSharedBufferGuid& mojo_guid,
    MojoPlatformSharedMemoryRegionAccessMode access) {
  if (mojo_platform_handles.empty() || mojo_platform_handles.size() > 2) {
    return nullptr;
  }

  using Mode = base::subtle::PlatformSharedMemoryRegion::Mode;
  Mode mode;
  switch (access) {
    case MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY:
      mode = Mode::kReadOnly;
      break;
    case MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE:
      mode = Mode::kWritable;
      break;
    case MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_UNSAFE:
      mode = Mode::kUnsafe;
      break;
    default:
      return nullptr;
  }

  std::optional<base::UnguessableToken> guid =
      base::UnguessableToken::Deserialize(mojo_guid.high, mojo_guid.low);
  if (!guid.has_value()) {
    return nullptr;
  }

  PlatformHandle handles[2];
  for (size_t i = 0; i < mojo_platform_handles.size(); ++i) {
    handles[i] =
        PlatformHandle::FromMojoPlatformHandle(&mojo_platform_handles[i]);
  }

  auto handle = CreateRegionHandleFromPlatformHandles(
      {&handles[0], mojo_platform_handles.size()}, mode);
  auto region = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(handle), mode, size, guid.value());
  if (!region.IsValid()) {
    return nullptr;
  }

  return base::MakeRefCounted<SharedBuffer>(std::move(region));
}

void SharedBuffer::Close() {
  region_ = {};
}

bool SharedBuffer::IsSerializable() const {
  return true;
}

bool SharedBuffer::GetSerializedDimensions(Transport& transmitter,
                                           size_t& num_bytes,
                                           size_t& num_handles) {
  num_bytes = sizeof(BufferHeader);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_ANDROID)
  num_handles = 1;
#else
  if (region_.GetMode() ==
      base::subtle::PlatformSharedMemoryRegion::Mode::kWritable) {
    num_handles = 2;
  } else {
    num_handles = 1;
  }
#endif
  return true;
}

bool SharedBuffer::Serialize(Transport& transmitter,
                             base::span<uint8_t> data,
                             base::span<PlatformHandle> handles) {
  if (!region_.IsValid()) {
    return false;
  }

  DCHECK_GE(data.size(), sizeof(BufferHeader));
  BufferHeader& header = *reinterpret_cast<BufferHeader*>(data.data());
  header.size = sizeof(header);
  header.buffer_size = static_cast<uint32_t>(region_.GetSize());
  header.padding = 0;
  switch (region_.GetMode()) {
    case base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly:
      header.mode = BufferMode::kReadOnly;
      break;
    case base::subtle::PlatformSharedMemoryRegion::Mode::kWritable:
      header.mode = BufferMode::kWritable;
      break;
    case base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe:
      header.mode = BufferMode::kUnsafe;
      break;
  }
  base::UnguessableToken guid = region_.GetGUID();
  header.guid_low = guid.GetLowForSerialization();
  header.guid_high = guid.GetHighForSerialization();

  auto handle = region_.PassPlatformHandle();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_ANDROID)
  DCHECK_EQ(handles.size(), 1u);
  handles[0] = PlatformHandle(std::move(handle));
#else
  if (header.mode == BufferMode::kWritable) {
    DCHECK_EQ(2u, handles.size());
    handles[0] = PlatformHandle(std::move(handle.fd));
    handles[1] = PlatformHandle(std::move(handle.readonly_fd));
  } else {
    DCHECK_EQ(1u, handles.size());
    handles[0] = PlatformHandle(std::move(handle.fd));
  }
#endif

  return true;
}

// static
scoped_refptr<SharedBuffer> SharedBuffer::Deserialize(
    base::span<const uint8_t> data,
    base::span<PlatformHandle> handles) {
  if (data.size() < sizeof(BufferHeader) || handles.empty()) {
    return nullptr;
  }

  const BufferHeader& header =
      *reinterpret_cast<const BufferHeader*>(data.data());
  const size_t header_size = header.size;
  if (header_size < sizeof(BufferHeader) || header_size % 8 != 0) {
    return nullptr;
  }

  base::subtle::PlatformSharedMemoryRegion::Mode mode;
  switch (header.mode) {
    case BufferMode::kReadOnly:
      mode = base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly;
      break;
    case BufferMode::kWritable:
      mode = base::subtle::PlatformSharedMemoryRegion::Mode::kWritable;
      break;
    case BufferMode::kUnsafe:
      mode = base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
      break;
    default:
      return nullptr;
  }

  std::optional<base::UnguessableToken> guid =
      base::UnguessableToken::Deserialize(header.guid_high, header.guid_low);
  if (!guid.has_value()) {
    return nullptr;
  }

  auto handle = CreateRegionHandleFromPlatformHandles(handles, mode);
  auto region = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(handle), mode, header.buffer_size, guid.value());
  if (!region.IsValid()) {
    return nullptr;
  }

  return base::MakeRefCounted<SharedBuffer>(std::move(region));
}

}  // namespace mojo::core::ipcz_driver
