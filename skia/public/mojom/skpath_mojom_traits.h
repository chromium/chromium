// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_SKPATH_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_SKPATH_MOJOM_TRAITS_H_

#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "skia/public/mojom/skpath.mojom-shared.h"
#include "third_party/skia/include/core/SkPath.h"

namespace mojo {

template <>
struct StructTraits<skia::mojom::SkPathDataView, ::SkPath> {
  static std::vector<uint8_t> data(const SkPath& path) {
    std::vector<uint8_t> buffer(path.writeToMemory(nullptr));
    CHECK_EQ(path.writeToMemory(buffer.data()), buffer.size());
    return buffer;
  }

  static bool Read(skia::mojom::SkPathDataView data, ::SkPath* path) {
    std::vector<uint8_t> buffer;
    return data.ReadData(&buffer) &&
           path->readFromMemory(buffer.data(), buffer.size());
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_SKPATH_MOJOM_TRAITS_H_
