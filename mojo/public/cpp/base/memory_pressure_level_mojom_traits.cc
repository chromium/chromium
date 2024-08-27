// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/memory_pressure_level_mojom_traits.h"

namespace mojo {

// static
mojo_base::mojom::MemoryPressureLevel
EnumTraits<mojo_base::mojom::MemoryPressureLevel,
           base::MemoryPressureListener::MemoryPressureLevel>::
    ToMojom(base::MemoryPressureListener::MemoryPressureLevel input) {
  switch (input) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return mojo_base::mojom::MemoryPressureLevel::NONE;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      return mojo_base::mojom::MemoryPressureLevel::MODERATE;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return mojo_base::mojom::MemoryPressureLevel::CRITICAL;
  }
  NOTREACHED();
}

// static
bool EnumTraits<mojo_base::mojom::MemoryPressureLevel,
                base::MemoryPressureListener::MemoryPressureLevel>::
    FromMojom(mojo_base::mojom::MemoryPressureLevel input,
              base::MemoryPressureListener::MemoryPressureLevel* output) {
  switch (input) {
    case mojo_base::mojom::MemoryPressureLevel::NONE:
      *output = base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_NONE;
      return true;
    case mojo_base::mojom::MemoryPressureLevel::MODERATE:
      *output = base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE;
      return true;
    case mojo_base::mojom::MemoryPressureLevel::CRITICAL:
      *output = base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL;
      return true;
  }
  return false;
}

}  // namespace mojo
