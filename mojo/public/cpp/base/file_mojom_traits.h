// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_FILE_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_FILE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "mojo/public/mojom/base/file.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::FileDataView, base::File> {
  static bool IsNull(const base::File& file) { return !file.IsValid(); }

  static void SetToNull(base::File* file) { *file = base::File(); }

  static mojo::PlatformHandle fd(base::File& file);
  static bool async(base::File& file) { return file.async(); }
  static bool Read(mojo_base::mojom::FileDataView data, base::File* file);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_FILE_MOJOM_TRAITS_H_
