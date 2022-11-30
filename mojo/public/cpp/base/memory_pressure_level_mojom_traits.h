// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_MEMORY_PRESSURE_LEVEL_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_MEMORY_PRESSURE_LEVEL_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/memory/memory_pressure_listener.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/memory_pressure_level.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::MemoryPressureLevel,
               base::MemoryPressureListener::MemoryPressureLevel> {
  static mojo_base::mojom::MemoryPressureLevel ToMojom(
      base::MemoryPressureListener::MemoryPressureLevel input);
  static bool FromMojom(
      mojo_base::mojom::MemoryPressureLevel input,
      base::MemoryPressureListener::MemoryPressureLevel* output);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_MEMORY_PRESSURE_LEVEL_MOJOM_TRAITS_H_
