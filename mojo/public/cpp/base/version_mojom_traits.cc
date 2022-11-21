// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/version_mojom_traits.h"

#include <cstdint>
#include <vector>

namespace mojo {

bool StructTraits<mojo_base::mojom::VersionDataView, base::Version>::Read(
    mojo_base::mojom::VersionDataView data,
    base::Version* out) {
  std::vector<uint32_t> components;
  if (!data.ReadComponents(&components))
    return false;

  *out = base::Version(std::move(components));
  return true;
}

}  // namespace mojo
