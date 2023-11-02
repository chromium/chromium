// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/file_info_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::FileInfoDataView, base::File::Info>::Read(
    mojo_base::mojom::FileInfoDataView data,
    base::File::Info* out) {
  if (!data.ReadLastModified(&out->last_modified))
    return false;
  if (!data.ReadLastAccessed(&out->last_accessed))
    return false;
  if (!data.ReadCreationTime(&out->creation_time))
    return false;
  out->size = data.size();
  out->is_directory = data.is_directory();
  out->is_symbolic_link = data.is_symbolic_link();
  return true;
}

}  // namespace mojo
