// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_ENVELOPE_H_
#define MOJO_CORE_IPCZ_DRIVER_ENVELOPE_H_

#include "mojo/buildflags.h"
#include "mojo/core/ipcz_driver/object.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#include "base/apple/scoped_mach_port.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#endif

namespace mojo::core::ipcz_driver {

// Used to keep platform specific data alive while processing and dispatching a
// message.
class MOJO_SYSTEM_IMPL_EXPORT Envelope : public Object<Envelope> {
 public:
  Envelope();
#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  explicit Envelope(
      base::apple::ScopedMachSendRight voucher,
      base::RepeatingCallback<void(base::apple::ScopedMachSendRight)>
          release_voucher_callback = base::NullCallback());
#endif

  static Type object_type() { return kEnvelope; }

  // ObjectBase:
  void Close() override;

 protected:
  // protected for tests
  ~Envelope() override;

 private:
#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  base::apple::ScopedMachSendRight voucher_;
  base::RepeatingCallback<void(base::apple::ScopedMachSendRight)>
      release_voucher_callback_;
#endif
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_ENVELOPE_H_
