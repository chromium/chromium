// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/delegated_ink_point_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    viz::mojom::DelegatedInkPointDataView,
    viz::DelegatedInkPoint>::Read(viz::mojom::DelegatedInkPointDataView data,
                                  viz::DelegatedInkPoint* out) {
  out->pointer_id_ = data.pointer_id();
  return data.ReadPoint(&out->point_) && data.ReadTimestamp(&out->timestamp_);
}

}  // namespace mojo
