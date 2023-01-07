// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_DRIVER_H_
#define MOJO_CORE_IPCZ_DRIVER_DRIVER_H_

#include "mojo/core/system_impl_export.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

// The IpczDriver implementation provided by Mojo. This driver uses a transport
// based on mojo::core::Channel, and shared memory is implemented using //base
// shared memory APIs.
//
// The driver also supports boxing of platform handles and shared memory regions
// to simplify the transition of the Mojo bindings implementation to ipcz.
MOJO_SYSTEM_IMPL_EXPORT extern const IpczDriver kDriver;

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_DRIVER_H_
