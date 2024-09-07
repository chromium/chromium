// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/transmissible_platform_handle.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace mojo::core::ipcz_driver {

TransmissiblePlatformHandle::TransmissiblePlatformHandle() = default;

TransmissiblePlatformHandle::TransmissiblePlatformHandle(PlatformHandle handle)
    : handle_(std::move(handle)) {
#if BUILDFLAG(IS_APPLE)
  // Only Mach port rights are supported as transmissible handles on macOS. To
  // transmit a file descriptor it must first be placed into a fileport.
  DCHECK(handle_.is_valid_mach_port());
#elif BUILDFLAG(IS_FUCHSIA)
  // Only zx handles are supported as transmissible handles on Fuchsia. To
  // transmit a file descriptor, its underlying fdio object must be extracted
  // and transmitted.
  DCHECK(handle_.is_valid_handle());
#elif BUILDFLAG(IS_WIN)
  // TransmissiblePlatformHandle is not used on Windows because there is no such
  // thing: handles are inlined as message data during driver object
  // serialization.
  NOTREACHED();
#endif
}

TransmissiblePlatformHandle::~TransmissiblePlatformHandle() = default;

void TransmissiblePlatformHandle::Close() {
  handle_.reset();
}

bool TransmissiblePlatformHandle::IsSerializable() const {
  return true;
}

bool TransmissiblePlatformHandle::GetSerializedDimensions(
    Transport& transmitter,
    size_t& num_bytes,
    size_t& num_handles) {
  DCHECK(handle_.is_valid());
  num_bytes = 0;
  num_handles = 1;
  return true;
}

bool TransmissiblePlatformHandle::Serialize(
    Transport& transmitter,
    base::span<uint8_t> data,
    base::span<PlatformHandle> handles) {
  DCHECK_EQ(0u, data.size());
  DCHECK(handle_.is_valid());
  DCHECK_EQ(1u, handles.size());
  handles[0] = std::move(handle_);
  return true;
}

// static
scoped_refptr<TransmissiblePlatformHandle>
TransmissiblePlatformHandle::Deserialize(base::span<const uint8_t> data,
                                         base::span<PlatformHandle> handles) {
  if (handles.size() != 1) {
    return nullptr;
  }
  return base::MakeRefCounted<TransmissiblePlatformHandle>(
      std::move(handles[0]));
}

}  // namespace mojo::core::ipcz_driver
