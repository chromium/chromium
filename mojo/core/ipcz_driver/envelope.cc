// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/envelope.h"

namespace mojo::core::ipcz_driver {

Envelope::Envelope() = default;

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
Envelope::Envelope(base::apple::ScopedMachSendRight voucher)
    : voucher_(std::move(voucher)) {}
#endif

Envelope::~Envelope() = default;

void Envelope::Close() {
#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  voucher_.reset();
#endif
}

}  // namespace mojo::core::ipcz_driver
