// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_FILE_INFO_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_FILE_INFO_MOJOM_TRAITS_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/files/file.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/file_info.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::FileInfoDataView, base::File::Info> {
  static int64_t size(const base::File::Info& info) { return info.size; }

  static bool is_directory(const base::File::Info& info) {
    return info.is_directory;
  }

  static bool is_symbolic_link(const base::File::Info& info) {
    return info.is_symbolic_link;
  }

  static base::Time last_modified(const base::File::Info& info) {
    return info.last_modified;
  }

  static base::Time last_accessed(const base::File::Info& info) {
    return info.last_accessed;
  }

  static base::Time creation_time(const base::File::Info& info) {
    return info.creation_time;
  }

  static bool Read(mojo_base::mojom::FileInfoDataView data,
                   base::File::Info* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_FILE_INFO_MOJOM_TRAITS_H_
