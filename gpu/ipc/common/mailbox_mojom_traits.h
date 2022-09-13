// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MAILBOX_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_MAILBOX_MOJOM_TRAITS_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/mailbox.mojom-shared.h"
#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::MailboxDataView, gpu::Mailbox> {
  static base::span<const int8_t> name(const gpu::Mailbox& mailbox) {
    return mailbox.name;
  }
  static bool Read(gpu::mojom::MailboxDataView data, gpu::Mailbox* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_MAILBOX_MOJOM_TRAITS_H_
