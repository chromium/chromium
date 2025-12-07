// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/byte_size_mojom_traits.h"

#include "base/byte_size.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::ByteSizeDataView, base::ByteSize>::Read(
    mojo_base::mojom::ByteSizeDataView data,
    base::ByteSize* byte_size) {
  // ByteSize holds a uint64_t, but any value over max(int64_t) is invalid, so
  // that every valid ByteSize can convert to ByteSizeDelta. The ByteSize
  // constructor will CHECK if given an out-of-range value.
  if (data.size() > base::ByteSize::Max().InBytes()) {
    return false;
  }
  *byte_size = base::ByteSize(data.size());
  return true;
}

// static
bool StructTraits<
    mojo_base::mojom::ByteSizeDeltaDataView,
    base::ByteSizeDelta>::Read(mojo_base::mojom::ByteSizeDeltaDataView data,
                               base::ByteSizeDelta* byte_size_delta) {
  *byte_size_delta = base::ByteSizeDelta(data.delta());
  return true;
}

}  // namespace mojo
