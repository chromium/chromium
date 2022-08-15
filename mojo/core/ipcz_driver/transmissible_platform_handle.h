// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_TRANSMISSIBLE_PLATFORM_HANDLE_H_
#define MOJO_CORE_IPCZ_DRIVER_TRANSMISSIBLE_PLATFORM_HANDLE_H_

#include <utility>

#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo::core::ipcz_driver {

// Driver object to hold a PlatformHandle which the platform's Channel
// implementation can transmit as-is, out-of-band from message data.
//
// TransmissiblePlatformHandle is the only type of driver object that can be
// emitted by the driver's Serialize(), and it's the only kind accepted by its
// Transmit().
//
// Note that this is never used on Windows, where handles are inlined as message
// data during serialization.
class MOJO_SYSTEM_IMPL_EXPORT TransmissiblePlatformHandle
    : public Object<TransmissiblePlatformHandle> {
 public:
  TransmissiblePlatformHandle();
  explicit TransmissiblePlatformHandle(PlatformHandle handle);

  PlatformHandle& handle() { return handle_; }

  PlatformHandle TakeHandle() { return std::move(handle_); }

  static constexpr Type object_type() { return kTransmissiblePlatformHandle; }

  // Object:
  void Close() override;

 private:
  ~TransmissiblePlatformHandle() override;

  PlatformHandle handle_;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_TRANSMISSIBLE_PLATFORM_HANDLE_H_
