// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/memory_pressure_level_mojom_traits.h"

namespace mojo {

// static
mojo_base::mojom::MemoryPressureLevel EnumTraits<
    mojo_base::mojom::MemoryPressureLevel,
    base::MemoryPressureLevel>::ToMojom(base::MemoryPressureLevel input) {
  switch (input) {
    case base::MEMORY_PRESSURE_LEVEL_NONE:
      return mojo_base::mojom::MemoryPressureLevel::NONE;
    case base::MEMORY_PRESSURE_LEVEL_MODERATE:
      return mojo_base::mojom::MemoryPressureLevel::MODERATE;
    case base::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return mojo_base::mojom::MemoryPressureLevel::CRITICAL;
  }
  NOTREACHED();
}

// static
bool EnumTraits<mojo_base::mojom::MemoryPressureLevel,
                base::MemoryPressureLevel>::
    FromMojom(mojo_base::mojom::MemoryPressureLevel input,
              base::MemoryPressureLevel* output) {
  switch (input) {
    case mojo_base::mojom::MemoryPressureLevel::NONE:
      *output = base::MEMORY_PRESSURE_LEVEL_NONE;
      return true;
    case mojo_base::mojom::MemoryPressureLevel::MODERATE:
      *output = base::MEMORY_PRESSURE_LEVEL_MODERATE;
      return true;
    case mojo_base::mojom::MemoryPressureLevel::CRITICAL:
      *output = base::MEMORY_PRESSURE_LEVEL_CRITICAL;
      return true;
  }
  return false;
}

}  // namespace mojo
