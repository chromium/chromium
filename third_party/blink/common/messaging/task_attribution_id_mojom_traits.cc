// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/task_attribution_id_mojom_traits.h"

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"

namespace mojo {

bool StructTraits<blink::mojom::TaskAttributionId::DataView,
                  blink::scheduler::TaskAttributionId>::
    Read(blink::mojom::TaskAttributionId::DataView data,
         blink::scheduler::TaskAttributionId* out) {
  *out = blink::scheduler::TaskAttributionId(data.value());
  return true;
}

}  // namespace mojo
