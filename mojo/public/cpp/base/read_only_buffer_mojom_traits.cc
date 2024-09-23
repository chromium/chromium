// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/read_only_buffer_mojom_traits.h"

namespace mojo {

bool StructTraits<mojo_base::mojom::ReadOnlyBufferDataView,
                  base::span<const uint8_t>>::
    Read(mojo_base::mojom::ReadOnlyBufferDataView input,
         base::span<const uint8_t>* out) {
  ArrayDataView<uint8_t> data_view;
  input.GetBufferDataView(&data_view);

  // NOTE: This output directly refers to memory owned by the message.
  // Therefore, the message must stay valid while the output is passed to the
  // user code.
  *out = base::span<const uint8_t>(data_view.data(), data_view.size());
  return true;
}

}  // namespace mojo