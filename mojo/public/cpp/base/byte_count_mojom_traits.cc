// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/byte_count_mojom_traits.h"

#include "base/byte_count.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::ByteCountDataView, base::ByteCount>::Read(
    mojo_base::mojom::ByteCountDataView data,
    base::ByteCount* byte_count) {
  *byte_count = base::ByteCount(data.count());
  return true;
}

}  // namespace mojo
