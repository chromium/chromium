// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_PUBLIC_CPP_BASE_FILE_PATH_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_FILE_PATH_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/file_path.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::FilePathDataView, base::FilePath> {
#if BUILDFLAG(IS_WIN)
  static base::span<const uint16_t> path(const base::FilePath& path) {
    return base::make_span(
        reinterpret_cast<const uint16_t*>(path.value().data()),
        path.value().size());
  }
#else
  static const base::FilePath::StringType& path(const base::FilePath& path) {
    return path.value();
  }
#endif

  static bool Read(mojo_base::mojom::FilePathDataView data,
                   base::FilePath* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::RelativeFilePathDataView, base::FilePath> {
#if BUILDFLAG(IS_WIN)
  static base::span<const uint16_t> path(const base::FilePath& path);
#else
  static const base::FilePath::StringType& path(const base::FilePath& path);
#endif

  static bool Read(mojo_base::mojom::RelativeFilePathDataView data,
                   base::FilePath* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_FILE_PATH_MOJOM_TRAITS_H_
