// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/renderer_process_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::RendererProcessDataView,
                  network::RendererProcess>::
    Read(network::mojom::RendererProcessDataView data,
         network::RendererProcess* out) {
  int32_t raw_process_id = data.process_id();
  // network::RendererProcess has a DCHECK for this being 0, and we also want
  // to additionally exclude any negative value which is more strict than the
  // checking in network::RendererProcess::is_null().
  if (raw_process_id <= 0) {
    return false;
  }
  network::RendererProcess process_id(raw_process_id);
  // Additional validity check just in case the
  // network::RendererProcess::is_null() definition changes in the future.
  if (!process_id) {
    return false;
  }
  *out = process_id;
  return true;
}

}  // namespace mojo
