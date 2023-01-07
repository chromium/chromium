// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_WRAPPED_PLATFORM_HANDLE_H_
#define MOJO_CORE_IPCZ_DRIVER_WRAPPED_PLATFORM_HANDLE_H_

#include <utility>

#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo::core::ipcz_driver {

// Driver object which wraps a single PlatformHandle. PlatformHandles wrapped by
// this object may not be immediately transmissible by the platform's Channel
// implementation, but they can be serialized into a something that is.
class MOJO_SYSTEM_IMPL_EXPORT WrappedPlatformHandle
    : public Object<WrappedPlatformHandle> {
 public:
  WrappedPlatformHandle();
  explicit WrappedPlatformHandle(PlatformHandle handle);

  PlatformHandle& handle() { return handle_; }

  PlatformHandle TakeHandle() { return std::move(handle_); }

  static constexpr Type object_type() { return kWrappedPlatformHandle; }

  // Object:
  void Close() override;
  bool IsSerializable() const override;
  bool GetSerializedDimensions(Transport& transmitter,
                               size_t& num_bytes,
                               size_t& num_handles) override;
  bool Serialize(Transport& transmitter,
                 base::span<uint8_t> data,
                 base::span<PlatformHandle> handles) override;

  static scoped_refptr<WrappedPlatformHandle> Deserialize(
      base::span<const uint8_t> data,
      base::span<PlatformHandle> handles);

 private:
  ~WrappedPlatformHandle() override;

  PlatformHandle handle_;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_WRAPPED_PLATFORM_HANDLE_H_
