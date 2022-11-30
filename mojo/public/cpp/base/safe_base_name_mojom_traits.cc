// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/safe_base_name_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::SafeBaseNameDataView, base::SafeBaseName>::
    Read(mojo_base::mojom::SafeBaseNameDataView data, base::SafeBaseName* out) {
  base::FilePath path;
  if (!data.ReadPath(&path))
    return false;

  if (path.BaseName() != path)
    return false;

  auto maybe_basename = base::SafeBaseName::Create(path);
  if (!maybe_basename)
    return false;

  *out = *maybe_basename;

  return true;
}

}  // namespace mojo