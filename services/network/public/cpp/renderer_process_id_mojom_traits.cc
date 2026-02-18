// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/renderer_process_id_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::RendererProcessIdDataView,
                  network::RendererProcessId>::
    Read(network::mojom::RendererProcessIdDataView data,
         network::RendererProcessId* out) {
  int32_t raw_process_id = data.process_id();
  // network::RendererProcessId has a DCHECK for this being 0, and we also want
  // to additionally exclude any negative value which is more strict than the
  // checking in network::RendererProcessId::is_null().
  if (raw_process_id <= 0) {
    return false;
  }
  network::RendererProcessId process_id(raw_process_id);
  // Additional validity check just in case the
  // network::RendererProcessId::is_null() definition changes in the future.
  if (!process_id) {
    return false;
  }
  *out = process_id;
  return true;
}

}  // namespace mojo
