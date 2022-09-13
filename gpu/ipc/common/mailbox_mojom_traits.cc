// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/mailbox_mojom_traits.h"

#include "base/containers/span.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::MailboxDataView, gpu::Mailbox>::Read(
    gpu::mojom::MailboxDataView data,
    gpu::Mailbox* out) {
  base::span<int8_t> mailbox_name(out->name);
  return data.ReadName(&mailbox_name);
}

}  // namespace mojo
