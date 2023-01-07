// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_SHARED_BUFFER_H_
#define MOJO_CORE_IPCZ_DRIVER_SHARED_BUFFER_H_

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo::core::ipcz_driver {

// A SharedBuffer operates like Mojo Core's shared buffer object, as a weird
// thin wrapper around base's shared memory APIs. This exists to allow those
// objects to be boxed and transmitted through ipcz portals.
class MOJO_SYSTEM_IMPL_EXPORT SharedBuffer : public Object<SharedBuffer> {
 public:
  explicit SharedBuffer(base::subtle::PlatformSharedMemoryRegion region);

  template <typename RegionType>
  static scoped_refptr<SharedBuffer> MakeForRegion(RegionType region) {
    return base::MakeRefCounted<SharedBuffer>(
        RegionType::TakeHandleForSerialization(std::move(region)));
  }

  static constexpr Type object_type() { return kSharedBuffer; }

  base::subtle::PlatformSharedMemoryRegion& region() { return region_; }

  // Duplicates this buffer to a new buffer and returns a ref to the new object.
  // The second return value is a result indicating success or failure. Anything
  // other than IPCZ_RESULT_OK implies failure, and buffer must be null.
  std::pair<scoped_refptr<SharedBuffer>, IpczResult> Duplicate(bool read_only);

  // Helper to support the MojoWrapPlatformSharedMemoryRegion API.
  static scoped_refptr<SharedBuffer> CreateForMojoWrapper(
      base::span<const MojoPlatformHandle> mojo_platform_handles,
      uint32_t size,
      const MojoSharedBufferGuid& mojo_guid,
      MojoPlatformSharedMemoryRegionAccessMode access);

  // Object:
  void Close() override;
  bool IsSerializable() const override;
  bool GetSerializedDimensions(Transport& transmitter,
                               size_t& num_bytes,
                               size_t& num_handles) override;
  bool Serialize(Transport& transmitter,
                 base::span<uint8_t> data,
                 base::span<PlatformHandle> handles) override;

  static scoped_refptr<SharedBuffer> Deserialize(
      base::span<const uint8_t> data,
      base::span<PlatformHandle> handles);

 private:
  ~SharedBuffer() override;

  base::subtle::PlatformSharedMemoryRegion region_;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_SHARED_BUFFER_H_
