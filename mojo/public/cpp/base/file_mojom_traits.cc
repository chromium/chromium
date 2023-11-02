// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "base/files/file.h"

namespace mojo {

mojo::PlatformHandle
StructTraits<mojo_base::mojom::FileDataView, base::File>::fd(base::File& file) {
  DCHECK(file.IsValid());

  return mojo::PlatformHandle(
      base::ScopedPlatformFile(file.TakePlatformFile()));
}

bool StructTraits<mojo_base::mojom::FileDataView, base::File>::Read(
    mojo_base::mojom::FileDataView data,
    base::File* file) {
  *file = base::File(data.TakeFd().TakePlatformFile(), data.async());
  return true;
}

}  // namespace mojo
