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
    if (!data.ReadData(&buffer)) {
      return false;
    }

    size_t bytes_deserialized = 0;
    if (auto deserialized_path = SkPath::ReadFromMemory(
            buffer.data(), buffer.size(), &bytes_deserialized)) {
      *path = *deserialized_path;
    }

    return !!bytes_deserialized;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_SKPATH_MOJOM_TRAITS_H_
