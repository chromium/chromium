// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/envelope.h"

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#include <utility>

#include "base/functional/callback.h"
#include "base/trace_event/typed_macros.h"
#endif

namespace mojo::core::ipcz_driver {

Envelope::Envelope() = default;

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
Envelope::Envelope(
    base::apple::ScopedMachSendRight voucher,
    base::RepeatingCallback<void(base::apple::ScopedMachSendRight)>
        release_voucher_callback)
    : voucher_(std::move(voucher)),
      release_voucher_callback_(std::move(release_voucher_callback)) {}
#endif

Envelope::~Envelope() = default;

void Envelope::Close() {
#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  if (release_voucher_callback_) {
    // The callback will log a "Release QoS Voucher" trace.
    release_voucher_callback_.Run(std::move(voucher_));
  } else {
    TRACE_EVENT("ipc", "Release QoS Voucher");
    voucher_.reset();
  }
#endif
}

}  // namespace mojo::core::ipcz_driver
