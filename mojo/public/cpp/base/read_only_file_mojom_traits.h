// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_READ_ONLY_FILE_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_READ_ONLY_FILE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "mojo/public/mojom/base/file.mojom-shared.h"

namespace mojo {

// True if the underlying handle is only readable. Where possible this excludes
// deletion, writing, truncation, append and other operations that might modify
// the underlying file. False if we can tell that the file could be modified.
// On platforms where we cannot test the handle, always returns true.
COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
bool IsReadOnlyFile(const base::File& file);

// True if `file` is backed by a physical file rather than e.g. a socket or a
// pipe. This may block, so use it carefully.
COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
bool IsPhysicalFile(const base::File& file);

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ReadOnlyFileDataView, base::File> {
  static bool IsNull(const base::File& file) { return !file.IsValid(); }

  static void SetToNull(base::File* file) { *file = base::File(); }

  static mojo::PlatformHandle fd(base::File& file);
  static bool async(base::File& file) { return file.async(); }
  static bool Read(mojo_base::mojom::ReadOnlyFileDataView data,
                   base::File* file);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_READ_ONLY_FILE_MOJOM_TRAITS_H_
