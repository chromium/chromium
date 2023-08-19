// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/uuid_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::UuidDataView, base::Uuid>::Read(
    mojo_base::mojom::UuidDataView data,
    base::Uuid* out) {
  std::string uuid;
  if (!data.ReadValue(&uuid)) {
    return false;
  }
  *out = base::Uuid::ParseLowercase(uuid);
  return out->is_valid();
}

}  // namespace mojo
