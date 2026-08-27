// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/memory_limit_mojom_traits.h"

#include "base/memory_coordinator/memory_limit.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::MemoryLimitDataView,
                  base::MemoryLimit>::Read(mojo_base::mojom::MemoryLimitDataView
                                               data,
                                           base::MemoryLimit* memory_limit) {
  if (data.percentage() < 0) {
    return false;
  }
  *memory_limit = base::MemoryLimit::FromPercent(data.percentage());
  return true;
}

}  // namespace mojo
