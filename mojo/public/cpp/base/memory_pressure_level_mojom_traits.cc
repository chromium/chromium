// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/memory_pressure_level_mojom_traits.h"

#include "base/notreached.h"

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
base::MemoryPressureLevel
EnumTraits<mojo_base::mojom::MemoryPressureLevel, base::MemoryPressureLevel>::
    FromMojom(mojo_base::mojom::MemoryPressureLevel input) {
  switch (input) {
    case mojo_base::mojom::MemoryPressureLevel::NONE:
      return base::MEMORY_PRESSURE_LEVEL_NONE;
    case mojo_base::mojom::MemoryPressureLevel::MODERATE:
      return base::MEMORY_PRESSURE_LEVEL_MODERATE;
    case mojo_base::mojom::MemoryPressureLevel::CRITICAL:
      return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  NOTREACHED();
}

}  // namespace mojo
